import 'dart:io';
import 'package:path_provider/path_provider.dart';
import 'bike_data.dart';

class RecordingService {
  List<BikeData> _buffer = [];
  bool _recording = false;

  bool get isRecording => _recording;
  int get frameCount => _buffer.length;

  void startRecording() {
    _buffer.clear();
    _recording = true;
  }

  void addFrame(BikeData data) {
    if (_recording) {
      _buffer.add(data);
    }
  }

  String stopRecording() {
    _recording = false;
    return _generateCsv();
  }

  String _generateCsv() {
    if (_buffer.isEmpty) return '';

    final lines = <String>[];
    lines.add('Time(ms),Speed(kmh),Cadence(rpm),Torque(raw),Power(W),Current(A),Voltage(V),Throttle(V),'
        'RawNorm,FilteredTorque,Target,CadenceGate');

    for (final d in _buffer) {
      final time = DateTime.now().millisecondsSinceEpoch;
      lines.add('$time,${d.speed.toStringAsFixed(1)},${d.cadence.toStringAsFixed(1)},'
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
    final timestamp = DateTime.now().toString().replaceAll(':', '-').split('.')[0];
    final filename = 'throttle_log_$timestamp.csv';

    try {
      final dir = await getApplicationDocumentsDirectory();
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
