import 'dart:io';
import 'dart:async';
import 'dart:typed_data';
import 'bike_data.dart';
import 'serial_base.dart';
import 'package:usb_serial/usb_serial.dart';

SerialService createSerialService() => _AndroidSerialService();

// ── Android — usb_serial (USB OTG / CDC-ACM) ──────────────────────────────
class _AndroidSerialService implements SerialService {
  UsbPort? _port;
  StreamSubscription<Uint8List>? _sub;
  String _buf = '';

  final _data   = StreamController<BikeData>.broadcast();
  final _status = StreamController<String>.broadcast();

  @override Stream<BikeData> get dataStream   => _data.stream;
  @override Stream<String>   get statusStream => _status.stream;
  @override bool get isConnected => _port != null;

  @override
  Future<List<PortInfo>> listPorts() async {
    final devices = await UsbSerial.listDevices();
    return devices
        .map((d) => PortInfo(
              d.deviceName ?? '',
              '${d.manufacturerName ?? ''} ${d.productName ?? ''}'.trim(),
            ))
        .toList();
  }

  @override
  Future<bool> connect(PortInfo port) async {
    try {
      final devices = await UsbSerial.listDevices();
      final dev = devices.firstWhere((d) => d.deviceName == port.id);
      _port = await dev.create();
      if (!await _port!.open()) {
        _status.add('Konnte USB-Gerät nicht öffnen');
        return false;
      }
      await _port!.setDTR(true);
      await _port!.setRTS(true);
      await _port!.setPortParameters(
          115200, UsbPort.DATABITS_8, UsbPort.STOPBITS_1, UsbPort.PARITY_NONE);
      _port!.write(Uint8List.fromList([0x3F]));
      _sub = _port!.inputStream!.listen(_onData,
          onError: (e) => _status.add('Fehler: $e'),
          onDone:  ()  => _status.add('Getrennt'));
      _status.add('Verbunden mit ${port.label}');
      return true;
    } catch (e) {
      _status.add('Exception: $e');
      return false;
    }
  }

  void _onData(Uint8List raw) {
    _buf += String.fromCharCodes(raw);
    final idx = _buf.lastIndexOf('\n');
    if (idx < 0) return;
    final lines = _buf.substring(0, idx).split('\n');
    _buf = _buf.substring(idx + 1);
    for (final l in lines) {
      final d = BikeData.tryParse(l.trim());
      if (d != null) _data.add(d);
    }
  }

  @override
  Future<void> disconnect() async {
    _port?.write(Uint8List.fromList([0x21]));
    await _sub?.cancel();
    await _port?.close();
    _port = null;
    _status.add('Getrennt');
  }

  @override
  void dispose() { disconnect(); _data.close(); _status.close(); }
}
