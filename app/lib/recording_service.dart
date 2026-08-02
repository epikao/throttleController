import 'dart:io';
import 'package:path_provider/path_provider.dart';
import 'bike_data.dart';

class _Frame {
  final int tMs;         // Empfangszeit (epoch ms) — beim Eintreffen erfasst
  final BikeData d;
  const _Frame(this.tMs, this.d);
}

class RecordingService {
  final List<_Frame> _buffer = [];
  bool _recording = false;

  bool get isRecording => _recording;
  int get frameCount => _buffer.length;

  void startRecording() {
    _buffer.clear();
    _recording = true;
  }

  void addFrame(BikeData data) {
    if (_recording) {
      _buffer.add(_Frame(DateTime.now().millisecondsSinceEpoch, data));
    }
  }

  void stopRecording() {
    _recording = false;
  }

  String _generateCsv() {
    if (_buffer.isEmpty) return '';

    final t0 = _buffer.first.tMs;
    final p = _buffer.first.d;  // Parameter-Snapshot vom Aufnahmestart
    final lines = <String>[];

    // Parameter-Kopf: '#'-Zeilen werden von Analyse-Tools (pandas: comment='#')
    // übersprungen; in Excel erscheinen sie als normale Zeilen vor den Daten.
    lines.add('# Aufnahme: ${DateTime.fromMillisecondsSinceEpoch(t0)}');
    lines.add('# Frames: ${_buffer.length}');
    lines.add('# --- Parameter bei Aufnahmestart ---');
    lines.add('# supportLevel=${p.support}, pasMode=${p.pasMode}, raceMode=${p.raceMode ? 1 : 0}');
    lines.add('# torqueZero=${p.torqueZero}, torqueMax=${p.torqueMax}, '
        'torqueDeadband=${p.torqueDeadband}, torqueIdleMs=${p.torqueIdleMs}');
    lines.add('# torqueFilterRise=${p.torqueFilterRise.toStringAsFixed(3)}, '
        'torqueFilterFall=${p.torqueFilterFall.toStringAsFixed(3)}');
    lines.add('# pulsesPerRev=${p.pulsesPerRev}, cadenceTimeoutMs=${p.cadenceTimeoutMs}, '
        'cadenceGatePulses=${p.cadenceGatePulses}, cadenceGateMs=${p.cadenceGateMs}');
    lines.add('# cadenceMaxRpm=${p.cadenceMaxRpm.toStringAsFixed(1)}, '
        'cadenceFilterAlpha=${p.cadenceFilterAlpha.toStringAsFixed(3)}, '
        'cadenceMaxLimit=${p.cadenceMaxLimit.toStringAsFixed(1)}');
    lines.add('# curveY25=${p.curveY25.toStringAsFixed(3)}, curveY50=${p.curveY50.toStringAsFixed(3)}, '
        'curveY75=${p.curveY75.toStringAsFixed(3)}, curveOffset=${p.curveOffset.toStringAsFixed(3)}');
    lines.add('# rampUpLow=${p.rampUpLow.toStringAsFixed(4)}, rampUpHigh=${p.rampUpHigh.toStringAsFixed(4)}, '
        'rampThreshold=${p.rampThreshold.toStringAsFixed(3)}, rampDown=${p.rampDown.toStringAsFixed(4)}');
    lines.add('# cruisePower=${p.cruisePower}, cruiseRampUp=${p.cruiseRampUp.toStringAsFixed(4)}, '
        'cruiseRampDown=${p.cruiseRampDown.toStringAsFixed(4)}, cruiseDeadband=${p.cruiseDeadband.toStringAsFixed(2)}');
    lines.add('# voltageMin=${p.voltageMin.toStringAsFixed(1)}, voltageMax=${p.voltageMax.toStringAsFixed(1)}, '
        'currentMax=${p.currentMax.toStringAsFixed(1)}, capacityMax=${p.capacityMax.toStringAsFixed(1)}');
    lines.add('# wheelCircle=${p.wheelCircle}');
    lines.add('# speedLimitStreet=${p.speedLimitStreet.toStringAsFixed(1)}, '
        'cruiseLimitStreet=${p.cruiseLimitStreet.toStringAsFixed(1)}, powerLimitStreet=${p.powerLimitStreet}');
    lines.add('# speedLimitRace=${p.speedLimitRace.toStringAsFixed(1)}, '
        'cruiseLimitRace=${p.cruiseLimitRace.toStringAsFixed(1)}, powerLimitRace=${p.powerLimitRace}');
    lines.add('# ---');

    lines.add('t_ms,Speed_kmh,Cadence_rpm,Torque_raw,Power_W,Current_A,Voltage_V,'
        'Throttle_V,RawNorm,FilteredTorque,Target,CadenceGate');

    for (final f in _buffer) {
      final d = f.d;
      lines.add('${f.tMs - t0},${d.speed.toStringAsFixed(1)},${d.cadence.toStringAsFixed(1)},'
          '${d.torque},${d.power},${d.current.toStringAsFixed(2)},${d.voltage.toStringAsFixed(2)},'
          '${d.throttle.toStringAsFixed(3)},${d.diagRawNorm.toStringAsFixed(3)},'
          '${d.diagFilteredTorque.toStringAsFixed(3)},${d.diagTarget.toStringAsFixed(3)},'
          '${d.diagCadenceGateOpen}');
    }

    return lines.join('\n');
  }

  Future<String?> saveToDisk() async {
    if (_buffer.isEmpty) return null;

    final csv = _generateCsv();
    final now = DateTime.now();
    final ts = '${now.year}-${now.month.toString().padLeft(2, '0')}-${now.day.toString().padLeft(2, '0')}'
        '_${now.hour.toString().padLeft(2, '0')}-${now.minute.toString().padLeft(2, '0')}-${now.second.toString().padLeft(2, '0')}';
    final filename = 'throttle_log_$ts.csv';

    try {
      // Android: app-spezifischer externer Speicher
      // (/storage/emulated/0/Android/data/<pkg>/files) — ohne Extra-Permissions
      // per USB-Dateimanager erreichbar. Sonst: Documents-Verzeichnis.
      Directory? dir;
      if (Platform.isAndroid) dir = await getExternalStorageDirectory();
      dir ??= await getApplicationDocumentsDirectory();
      final file = File('${dir.path}/$filename');
      await file.writeAsString(csv);
      return file.path;
    } catch (e) {
      return null;
    }
  }

  void clear() {
    _buffer.clear();
    _recording = false;
  }
}
