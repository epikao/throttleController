export 'serial_base.dart';

import 'serial_base.dart';
import '_serial_native.dart'
    if (dart.library.js_interop) '_serial_web.dart'
    as _impl;

SerialService createSerialService() => _impl.createSerialService();
