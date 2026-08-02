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
    final lines = <String>[];
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
