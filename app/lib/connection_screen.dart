import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/material.dart';
import 'serial_service.dart';
import 'bike_data.dart';
import 'dashboard_screen.dart';
import 'recording_service.dart';

class ConnectionScreen extends StatefulWidget {
  const ConnectionScreen({super.key});
  @override
  State<ConnectionScreen> createState() => _ConnectionScreenState();
}

class _ConnectionScreenState extends State<ConnectionScreen> {
  late final SerialService _svc;
  late final RecordingService _recording;
  List<PortInfo> _ports      = [];
  PortInfo?      _selected;
  bool           _connecting = false;
  bool           _navigating = false;  // true → don't dispose on screen pop
  String         _status     = '';

  @override
  void initState() {
    super.initState();
    _svc = createSerialService();
    _recording = RecordingService();
    _svc.statusStream.listen((s) => setState(() => _status = s));
    if (!kIsWeb) _refresh();
  }

  Future<void> _refresh() async {
    final p = await _svc.listPorts();
    setState(() {
      _ports = p;
      if (_selected != null && !p.any((x) => x.id == _selected!.id)) {
        _selected = null;
      }
    });
  }

  Future<void> _connect([PortInfo? webPort]) async {
    final port = webPort ?? _selected;
    if (port == null) return;
    setState(() => _connecting = true);
    final ok = await _svc.connect(port);
    if (!mounted) return;
    if (ok) {
      _navigating = true;
      final svc = _svc;
      final rec = _recording;
      svc.dataStream.listen((data) => rec.addFrame(data));
      Navigator.pushReplacement(
        context,
        MaterialPageRoute(
          builder: (_) => StreamBuilder<BikeData>(
            stream:      svc.dataStream,
            initialData: const BikeData(),
            builder: (ctx, snap) => DashboardScreen(
              data: snap.data ?? const BikeData(),
              recording: rec,
              onDisconnect: () async {
                await svc.disconnect();
                svc.dispose();
                if (ctx.mounted) {
                  Navigator.pushReplacement(ctx,
                      MaterialPageRoute(builder: (_) => const ConnectionScreen()));
                }
              },
            ),
          ),
        ),
      );
    }
    if (mounted) setState(() => _connecting = false);
  }

  @override
  void dispose() { if (!_navigating) _svc.dispose(); super.dispose(); }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF1A1A2E),
      appBar: AppBar(
        backgroundColor: const Color(0xFF16213E),
        title: const Text('eBike Dashboard', style: TextStyle(color: Colors.white)),
        actions: [
          if (!kIsWeb)
            IconButton(
              icon: const Icon(Icons.refresh, color: Colors.white70),
              onPressed: _refresh,
            ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(20),
        child: kIsWeb ? _buildWeb() : _buildNative(),
      ),
    );
  }

  // ── Web: single button (browser shows port picker dialog) ────────────────
  Widget _buildWeb() => Column(
    mainAxisAlignment: MainAxisAlignment.center,
    crossAxisAlignment: CrossAxisAlignment.stretch,
    children: [
      const Icon(Icons.usb, size: 64, color: Colors.cyan),
      const SizedBox(height: 20),
      const Text('Controller per USB verbinden,\ndann auf "Verbinden" tippen.',
          textAlign: TextAlign.center,
          style: TextStyle(color: Colors.white70, fontSize: 16)),
      const SizedBox(height: 8),
      const Text('Chrome öffnet einen Port-Auswahl-Dialog.',
          textAlign: TextAlign.center,
          style: TextStyle(color: Colors.white38, fontSize: 13)),
      const SizedBox(height: 32),
      if (_status.isNotEmpty)
        Padding(
          padding: const EdgeInsets.only(bottom: 12),
          child: Text(_status,
              style: const TextStyle(color: Colors.white60),
              textAlign: TextAlign.center),
        ),
      ElevatedButton.icon(
        onPressed: _connecting ? null : () => _connect(const PortInfo('__web__', '')),
        icon: _connecting
            ? const SizedBox(width: 18, height: 18,
                child: CircularProgressIndicator(strokeWidth: 2))
            : const Icon(Icons.usb),
        label: Text(_connecting ? 'Verbinde...' : 'Verbinden'),
        style: ElevatedButton.styleFrom(
          backgroundColor: Colors.cyan[800],
          foregroundColor: Colors.white,
          padding: const EdgeInsets.symmetric(vertical: 14),
        ),
      ),
    ],
  );

  // ── Native: port list ────────────────────────────────────────────────────
  Widget _buildNative() => Column(
    crossAxisAlignment: CrossAxisAlignment.stretch,
    children: [
      const Text('Port auswählen:', style: TextStyle(color: Colors.white70, fontSize: 16)),
      const SizedBox(height: 12),
      if (_ports.isEmpty)
        const Center(
            child: Text('Keine Ports gefunden',
                style: TextStyle(color: Colors.redAccent)))
      else
        ..._ports.map((p) => RadioListTile<PortInfo>(
          value:      p,
          groupValue: _selected,
          onChanged:  (v) => setState(() => _selected = v),
          title:      Text(p.label, style: const TextStyle(color: Colors.white)),
          subtitle:   p.label != p.id
              ? Text(p.id, style: const TextStyle(color: Colors.white38, fontSize: 12))
              : null,
          activeColor: Colors.cyanAccent,
        )),
      const Spacer(),
      if (_status.isNotEmpty)
        Padding(
          padding: const EdgeInsets.only(bottom: 12),
          child: Text(_status,
              style: const TextStyle(color: Colors.white60),
              textAlign: TextAlign.center),
        ),
      ElevatedButton.icon(
        onPressed: (_selected == null || _connecting) ? null : _connect,
        icon: _connecting
            ? const SizedBox(width: 18, height: 18,
                child: CircularProgressIndicator(strokeWidth: 2))
            : const Icon(Icons.usb),
        label: Text(_connecting ? 'Verbinde...' : 'Verbinden'),
        style: ElevatedButton.styleFrom(
          backgroundColor: Colors.cyan[800],
          foregroundColor: Colors.white,
          padding: const EdgeInsets.symmetric(vertical: 14),
        ),
      ),
    ],
  );
}
