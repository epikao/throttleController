import 'dart:io';
import 'package:path_provider/path_provider.dart';
import 'bike_data.dart';

/// Schreibt die Aufnahme LAUFEND auf die Platte und flusht regelmaessig.
///
/// Wichtig: Dart schreibt per Default OHNE flush - die Daten gelten als
/// geschrieben, sobald das OS sie im Puffer hat, nicht wenn sie im Flash
/// liegen. Wird die App vorher gekillt (MIUI macht das gern), hat die Datei
/// die volle Groesse, aber keinen Inhalt. Deshalb: alle FLUSH_EVERY Frames
/// hart auf die Platte zwingen. Schlimmstenfalls gehen so die letzten
/// Sekunden verloren statt der ganzen Fahrt.
///
/// Nebeneffekt: Es liegt nie mehr als ein Frame im RAM -> die Aufnahmedauer
/// ist nur noch durch den freien Speicher begrenzt (~20 MB pro Stunde).
class RecordingService {
  bool    _recording  = false;
  File?   _file;
  IOSink? _sink;
  int     _count      = 0;
  int?    _t0;              // Empfangszeit des ersten Frames (epoch ms)
  bool    _headerDone = false;
  int     _sinceFlush = 0;
  bool    _flushing   = false;

  static const int _flushEvery = 250;   // ~5 s bei 50 Hz

  bool get isRecording => _recording;
  int  get frameCount  => _count;

  /// Legt die Datei an und oeffnet den Schreibkanal. false = fehlgeschlagen.
  Future<bool> startRecording() async {
    if (_recording) return true;
    _count      = 0;
    _t0         = null;
    _headerDone = false;
    _sinceFlush = 0;
    _flushing   = false;
    try {
      // Android: app-spezifischer externer Speicher
      // (/storage/emulated/0/Android/data/<pkg>/files) - ohne Extra-Permissions
      // per USB-Dateimanager erreichbar. Sonst: Documents-Verzeichnis.
      Directory? dir;
      if (Platform.isAndroid) dir = await getExternalStorageDirectory();
      dir ??= await getApplicationDocumentsDirectory();
      _file = File('${dir.path}/${_filename()}');
      _sink = _file!.openWrite();
      _recording = true;
      return true;
    } catch (_) {
      _sink = null;
      _file = null;
      _recording = false;
      return false;
    }
  }

  void addFrame(BikeData d) {
    final sink = _sink;
    if (!_recording || sink == null) return;

    // Zeitstempel beim EINTREFFEN erfassen, nicht beim Speichern.
    final t = DateTime.now().millisecondsSinceEpoch;
    _t0 ??= t;

    // Der Parameter-Kopf kommt aus dem ersten Frame der Aufnahme.
    if (!_headerDone) {
      sink.write(_header(d, _t0!));
      _headerDone = true;
    }
    sink.writeln(_row(t - _t0!, d));
    _count++;

    // Guard: nie zwei Flushes gleichzeitig anstossen.
    if (++_sinceFlush >= _flushEvery && !_flushing) {
      _sinceFlush = 0;
      _flushing   = true;
      sink.flush().whenComplete(() => _flushing = false);
    }
  }

  /// Beendet die Aufnahme. Gibt den Dateipfad zurueck, oder null wenn
  /// nichts aufgenommen wurde (die leere Datei wird dann geloescht).
  Future<String?> stopRecording() async {
    _recording = false;
    final sink = _sink;
    final file = _file;
    _sink = null;
    _file = null;
    if (sink == null) return null;

    try {
      await sink.flush();
      await sink.close();
    } catch (_) {
      return null;
    }

    if (_count == 0) {
      try { await file?.delete(); } catch (_) {}
      return null;
    }
    return file?.path;
  }

  // --- Formatierung ---------------------------------------------------------

  static String _filename() {
    final n = DateTime.now();
    String p(int v) => v.toString().padLeft(2, '0');
    return 'throttle_log_${n.year}-${p(n.month)}-${p(n.day)}'
        '_${p(n.hour)}-${p(n.minute)}-${p(n.second)}.csv';
  }

  /// Parameter-Kopf: '#'-Zeilen werden von Analyse-Tools (pandas: comment='#')
  /// uebersprungen; in Excel erscheinen sie als normale Zeilen vor den Daten.
  static String _header(BikeData p, int t0) {
    final b = StringBuffer();
    void l(String s) => b.writeln('# $s');

    l('Aufnahme: ${DateTime.fromMillisecondsSinceEpoch(t0)}');
    l('--- Parameter bei Aufnahmestart ---');
    l('supportLevel=${p.support}, pasMode=${p.pasMode}, raceMode=${p.raceMode ? 1 : 0}');
    l('torqueZero=${p.torqueZero}, torqueMax=${p.torqueMax}, '
      'torqueDeadband=${p.torqueDeadband}, torqueIdleMs=${p.torqueIdleMs}');
    l('torqueFilterRise=${p.torqueFilterRise.toStringAsFixed(3)}, '
      'torqueFilterFall=${p.torqueFilterFall.toStringAsFixed(3)}');
    l('pulsesPerRev=${p.pulsesPerRev}, cadenceTimeoutMs=${p.cadenceTimeoutMs}, '
      'cadenceGatePulses=${p.cadenceGatePulses}, cadenceGateMs=${p.cadenceGateMs}');
    l('cadenceMaxRpm=${p.cadenceMaxRpm.toStringAsFixed(1)}, '
      'cadenceFilterAlpha=${p.cadenceFilterAlpha.toStringAsFixed(3)}, '
      'cadenceMaxLimit=${p.cadenceMaxLimit.toStringAsFixed(1)}');
    l('curveY25=${p.curveY25.toStringAsFixed(3)}, curveY50=${p.curveY50.toStringAsFixed(3)}, '
      'curveY75=${p.curveY75.toStringAsFixed(3)}, curveOffset=${p.curveOffset.toStringAsFixed(3)}');
    l('rampUpLow=${p.rampUpLow.toStringAsFixed(4)}, rampUpHigh=${p.rampUpHigh.toStringAsFixed(4)}, '
      'rampThreshold=${p.rampThreshold.toStringAsFixed(3)}, rampDown=${p.rampDown.toStringAsFixed(4)}');
    l('cruisePower=${p.cruisePower}, cruiseRampUp=${p.cruiseRampUp.toStringAsFixed(4)}, '
      'cruiseRampDown=${p.cruiseRampDown.toStringAsFixed(4)}, '
      'cruiseDeadband=${p.cruiseDeadband.toStringAsFixed(2)}');
    l('voltageMin=${p.voltageMin.toStringAsFixed(1)}, voltageMax=${p.voltageMax.toStringAsFixed(1)}, '
      'currentMax=${p.currentMax.toStringAsFixed(1)}, capacityMax=${p.capacityMax.toStringAsFixed(1)}');
    l('wheelCircle=${p.wheelCircle}');
    l('speedLimitStreet=${p.speedLimitStreet.toStringAsFixed(1)}, '
      'cruiseLimitStreet=${p.cruiseLimitStreet.toStringAsFixed(1)}, '
      'powerLimitStreet=${p.powerLimitStreet}');
    l('speedLimitRace=${p.speedLimitRace.toStringAsFixed(1)}, '
      'cruiseLimitRace=${p.cruiseLimitRace.toStringAsFixed(1)}, '
      'powerLimitRace=${p.powerLimitRace}');
    l('---');

    b.writeln('t_ms,Speed_kmh,Cadence_rpm,Torque_raw,Power_W,Current_A,Voltage_V,'
        'Throttle_V,RawNorm,FilteredTorque,GatedTorque,Target,CadenceGate,'
        'IMotor_A,Duty,ERPM,TMotor_C,TFet_C');
    return b.toString();
  }

  static String _row(int tMs, BikeData d) =>
      '$tMs,${d.speed.toStringAsFixed(1)},${d.cadence.toStringAsFixed(1)},'
      '${d.torque},${d.power},${d.current.toStringAsFixed(2)},'
      '${d.voltage.toStringAsFixed(2)},${d.throttle.toStringAsFixed(3)},'
      '${d.diagRawNorm.toStringAsFixed(3)},${d.diagFilteredTorque.toStringAsFixed(3)},'
      '${d.diagGatedTorque.toStringAsFixed(3)},'
      '${d.diagTarget.toStringAsFixed(3)},${d.diagCadenceGateOpen},'
      '${d.motorCurrent.toStringAsFixed(2)},${d.dutyCycle.toStringAsFixed(3)},'
      '${d.erpm.toStringAsFixed(0)},${d.tempMotor.toStringAsFixed(1)},'
      '${d.tempFet.toStringAsFixed(1)}';
}
