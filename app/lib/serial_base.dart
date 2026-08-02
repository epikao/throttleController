import 'dart:async';
import 'bike_data.dart';

class PortInfo {
  final String id;
  final String label;
  const PortInfo(this.id, this.label);
}

abstract class SerialService {
  Stream<BikeData> get dataStream;
  Stream<String>   get statusStream;
  bool get isConnected;
  Future<List<PortInfo>> listPorts();
  Future<bool>           connect(PortInfo port);
  Future<void>           disconnect();
  void                   dispose();
}
