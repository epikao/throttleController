import 'dart:async';
import 'dart:js_interop';
import 'dart:typed_data';
import 'bike_data.dart';
import 'serial_base.dart';

SerialService createSerialService() => _WebSerialService();

// ── JS interop type declarations ──────────────────────────────────────────

@JS()
external _Navigator get navigator;

extension type _Navigator(JSObject _) implements JSObject {
  external _Serial get serial;
}

extension type _Serial(JSObject _) implements JSObject {
  external JSPromise<JSArray<_SerialPort>> getPorts();
  external JSPromise<_SerialPort> requestPort();
}

extension type _SerialPort(JSObject _) implements JSObject {
  external JSPromise<JSAny?> open(JSObject options);
  external JSPromise<JSAny?> close();
  external _ReadableStream get readable;
  external _WritableStream get writable;
}

extension type _ReadableStream(JSObject _) implements JSObject {
  external _Reader getReader();
}

extension type _WritableStream(JSObject _) implements JSObject {
  external _Writer getWriter();
}

extension type _Reader(JSObject _) implements JSObject {
  external JSPromise<_ReadResult> read();
  external void releaseLock();
}

extension type _Writer(JSObject _) implements JSObject {
  external JSPromise<JSAny?> write(JSUint8Array chunk);
  external void releaseLock();
}

extension type _ReadResult(JSObject _) implements JSObject {
  external JSUint8Array? get value;
  external bool get done;
}

// ── Web Serial Service ────────────────────────────────────────────────────

class _WebSerialService implements SerialService {
  _SerialPort? _port;
  _Reader?     _reader;
  _Writer?     _writer;
  bool         _open = false;
  String       _buf  = '';

  final _data   = StreamController<BikeData>.broadcast();
  final _status = StreamController<String>.broadcast();

  @override Stream<BikeData> get dataStream   => _data.stream;
  @override Stream<String>   get statusStream => _status.stream;
  @override bool get isConnected => _open;

  @override
  Future<List<PortInfo>> listPorts() async =>
      // Web Serial: port is selected via browser permission dialog on connect
      [const PortInfo('__web__', 'Port auswählen (Browser-Dialog)')];

  @override
  Future<bool> connect(PortInfo port) async {
    try {
      _port = await navigator.serial.requestPort().toDart;
      final opts = {'baudRate': 115200}.jsify()! as JSObject;
      await _port!.open(opts).toDart;
      _writer = _port!.writable.getWriter();
      _reader = _port!.readable.getReader();
      _open   = true;
      await _writer!.write(Uint8List.fromList([0x3F]).toJS).toDart; // '?'
      _status.add('Verbunden');
      _readLoop();
      return true;
    } catch (e) {
      _status.add('Fehler: $e');
      return false;
    }
  }

  void _readLoop() async {
    while (_open) {
      try {
        final res = await _reader!.read().toDart;
        if (res.done) { _open = false; _status.add('Getrennt'); break; }
        final bytes = res.value!.toDart;
        _buf += String.fromCharCodes(bytes);
        final idx = _buf.lastIndexOf('\n');
        if (idx >= 0) {
          final lines = _buf.substring(0, idx).split('\n');
          _buf = _buf.substring(idx + 1);
          for (final l in lines) {
            final d = BikeData.tryParse(l.trim());
            if (d != null) _data.add(d);
          }
        }
      } catch (e) {
        _status.add('Lesefehler: $e');
        break;
      }
    }
  }

  @override
  Future<void> disconnect() async {
    _open = false;
    try {
      await _writer?.write(Uint8List.fromList([0x21]).toJS).toDart; // '!'
    } catch (_) {}
    _reader?.releaseLock();
    _writer?.releaseLock();
    await _port?.close().toDart;
    _port = null; _reader = null; _writer = null;
    _status.add('Getrennt');
  }

  @override
  void dispose() { disconnect(); _data.close(); _status.close(); }
}
