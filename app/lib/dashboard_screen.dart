import 'package:flutter/material.dart';
import 'bike_data.dart';
import 'recording_service.dart';

// ── Page names ────────────────────────────────────────────────────────────
const _pageNames = [
  'MAIN', 'TORQUE PAR', 'CADENCE PAR', 'THROTTLE CURVE',
  'PAS & BATTERY', 'DIV PARAMETERS', 'STOPWATCH',
  'REAR LIGHT', 'FRONT LIGHT',
];

class DashboardScreen extends StatefulWidget {
  final BikeData      data;
  final RecordingService recording;
  final VoidCallback  onDisconnect;
  const DashboardScreen({super.key, required this.data, required this.recording, required this.onDisconnect});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  late BikeData _currentData;

  @override
  void initState() {
    super.initState();
    _currentData = widget.data;
  }

  @override
  void didUpdateWidget(covariant DashboardScreen oldWidget) {
    super.didUpdateWidget(oldWidget);
    _currentData = widget.data;
  }

  static const _bg   = Color(0xFF1A1A2E);
  static const _card = Color(0xFF16213E);

  @override
  Widget build(BuildContext context) {
    final pageName = _currentData.page < _pageNames.length ? _pageNames[_currentData.page] : 'PAGE ${_currentData.page}';
    return Scaffold(
      backgroundColor: _bg,
      appBar: AppBar(
        backgroundColor: _card,
        titleSpacing: 12,
        title: Row(children: [
          Text(pageName, style: const TextStyle(color: Colors.white, fontSize: 15,
              fontWeight: FontWeight.bold)),
          const SizedBox(width: 10),
          _badge('SUP ${_currentData.support}', _currentData.menuState == 1 ? Colors.red[700]! : Colors.cyan[700]!),
          const SizedBox(width: 6),
          if (_currentData.raceMode) _badge('RACE', Colors.red[700]!),
          if (_currentData.cruise) ...[const SizedBox(width: 6), _badge('CRUISE', Colors.red[700]!)],
        ]),
        actions: [
          if (widget.recording.isRecording)
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 8.0),
              child: Center(
                child: Text('REC ${widget.recording.frameCount}',
                    style: const TextStyle(color: Colors.red, fontWeight: FontWeight.bold)),
              ),
            ),
          IconButton(
            icon: Icon(widget.recording.isRecording ? Icons.stop : Icons.fiber_manual_record,
                color: widget.recording.isRecording ? Colors.red : Colors.white54),
            onPressed: _toggleRecording,
            tooltip: widget.recording.isRecording ? 'Aufnahme stoppen' : 'Aufnahme starten',
          ),
          IconButton(icon: const Icon(Icons.link_off, color: Colors.white54),
              onPressed: widget.onDisconnect, tooltip: 'Trennen'),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.fromLTRB(8, 8, 8, 16),
        child: _buildPage(),
      ),
    );
  }

  void _toggleRecording() async {
    String? msg;
    if (widget.recording.isRecording) {
      final path = await widget.recording.stopRecording();
      msg = (path != null) ? 'CSV gespeichert: $path'
                           : 'Aufnahme leer - nichts gespeichert';
    } else {
      if (!await widget.recording.startRecording()) {
        msg = 'Aufnahme konnte nicht gestartet werden';
      }
    }
    if (!mounted) return;
    if (msg != null) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));
    }
    setState(() {});
  }

  Widget _buildPage() {
    switch (_currentData.page) {
      case 0:  return _PageMain(data: _currentData);
      case 1:  return _PageParams(data: _currentData, params: _torqueParams());
      case 2:  return _PageParams(data: _currentData, params: _cadenceParams());
      case 3:  return _PageParams(data: _currentData, params: _curveParams());
      case 4:  return _PageParams(data: _currentData, params: _pasBattParams());
      case 5:  return _PageDiv(data: _currentData);
      case 6:  return _PageStopwatch(data: _currentData);
      case 7:  return _PageRearLight(data: _currentData);
      case 8:  return _PageFrontLight(data: _currentData);
      default: return Center(child: Text('Page ${_currentData.page}',
                   style: const TextStyle(color: Colors.white54)));
    }
  }

  List<_Param> _torqueParams() => [
    _Param('TorqueZero',   '${_currentData.torqueZero}',                      11),
    _Param('TorqueMax',    '${_currentData.torqueMax}',                        12),
    _Param('Deadband',     '${_currentData.torqueDeadband}',                   13),
    _Param('FilterRise',   _currentData.torqueFilterRise.toStringAsFixed(3),   14),
    _Param('FilterFall',   _currentData.torqueFilterFall.toStringAsFixed(3),   15),
    _Param('TorqueIdle',   '${_currentData.torqueIdleMs} ms',                  16),
  ];

  List<_Param> _cadenceParams() => [
    _Param('PulsesPerRev', '${_currentData.pulsesPerRev}',                     21),
    _Param('TimeoutMs',    '${_currentData.cadenceTimeoutMs}',                 22),
    _Param('MaxRPM',       _currentData.cadenceMaxRpm.toStringAsFixed(1),      23),
    _Param('FilterAlpha',  _currentData.cadenceFilterAlpha.toStringAsFixed(3), 24),
    _Param('MaxLimit',     _currentData.cadenceMaxLimit.toStringAsFixed(1),    25),
    _Param('GatePulses',   '${_currentData.cadenceGatePulses}',                26),
    _Param('GateTime',     '${_currentData.cadenceGateMs} ms',                 27),
  ];

  List<_Param> _curveParams() => [
    _Param('CurveY25',      _currentData.curveY25.toStringAsFixed(3),          31),
    _Param('CurveY50',      _currentData.curveY50.toStringAsFixed(3),          32),
    _Param('CurveY75',      _currentData.curveY75.toStringAsFixed(3),          33),
    _Param('CurveOffset',   _currentData.curveOffset.toStringAsFixed(3),       34),
    _Param('RampUpLow',     _currentData.rampUpLow.toStringAsFixed(3),         35),
    _Param('RampUpHigh',    _currentData.rampUpHigh.toStringAsFixed(3),        36),
    _Param('RampThreshold', _currentData.rampThreshold.toStringAsFixed(3),     37),
    _Param('RampDown',      _currentData.rampDown.toStringAsFixed(3),          38),
  ];

  List<_Param> _pasBattParams() => [
    _Param('CruisePower',    '${_currentData.cruisePower} %',                    41),
    _Param('CruiseRampUp',   _currentData.cruiseRampUp.toStringAsFixed(3),       42),
    _Param('CruiseRampDown', _currentData.cruiseRampDown.toStringAsFixed(3),         43),
    _Param('CruiseDeadband', _currentData.cruiseDeadband.toStringAsFixed(2),           44),
    _Param('PasMode',        '${_currentData.pasMode}',                              45),
    _Param('VoltageMin',     '${_currentData.voltageMin.toStringAsFixed(1)} V',      46),
    _Param('VoltageMax',     '${_currentData.voltageMax.toStringAsFixed(1)} V',      47),
    _Param('CurrentMax',     '${_currentData.currentMax.toStringAsFixed(1)} A',      48),
    _Param('CapacityMax',    '${_currentData.capacityMax.toStringAsFixed(1)} Wh',    49),
  ];

  static Widget _badge(String text, Color color) => Container(
    padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 3),
    decoration: BoxDecoration(
      color: color.withOpacity(0.25), border: Border.all(color: color),
      borderRadius: BorderRadius.circular(4),
    ),
    child: Text(text, style: TextStyle(color: color, fontSize: 11, fontWeight: FontWeight.bold)),
  );
}

// ── Page 0: MAIN ──────────────────────────────────────────────────────────
class _PageMain extends StatelessWidget {
  final BikeData data;
  const _PageMain({required this.data});

  @override
  Widget build(BuildContext context) => Column(children: [
    _SpeedCard(speed: data.speed, raceMode: data.raceMode),
    const SizedBox(height: 6),
    _Row3(_MC('Power',    '${data.power}',                        'W'),
          _MC('Current',  data.current.toStringAsFixed(1),        'A'),
          _MC('Voltage',  data.voltage.toStringAsFixed(1),        'V')),
    const SizedBox(height: 6),
    _Row3(_MC('Cadence',  data.cadence.toStringAsFixed(0),        'rpm'),
          _MC('Torque',   '${data.torque}',                       ''),
          _MC('Throttle', data.throttle.toStringAsFixed(2),       'V')),
    const SizedBox(height: 6),
    _Row3(_MC('Temp',     data.temp.toStringAsFixed(1),           '°C'),
          _MC('Capacity', '${data.capacity}',                     '%'),
          _MC('Range',    '${data.range}',                        'km')),
    const SizedBox(height: 6),
    _Row3(_MC('Trip',     data.trip.toStringAsFixed(1),           'km'),
          _MC('Odo',      '${data.odo}',                          'km'),
          _MC('Wh/km',    data.whKm.toStringAsFixed(1),           '')),
    const SizedBox(height: 6),
    _Row3(_MC('Avg Speed',data.speedAvg.toStringAsFixed(1),       'km/h'),
          _MC('OnTime',   data.onTimeStr,                         ''),
          _MC('Time', data.rideStr,                           '')),
    if (data.stopwatchState > 0) ...[
      const SizedBox(height: 6),
      _StopwatchCard(data: data),
    ],
  ]);
}

// ── Pages 1–4: Live block + parameter list ────────────────────────────────
class _Param {
  final String label, value;
  final int    editCase;
  const _Param(this.label, this.value, [this.editCase = -1]);
}

class _PageParams extends StatelessWidget {
  final BikeData     data;
  final List<_Param> params;
  const _PageParams({required this.data, required this.params});

  @override
  Widget build(BuildContext context) => Column(children: [
    // Live block
    _Row3(_MC('Speed',   '${data.speed.toStringAsFixed(1)}', 'km/h'),
          _MC('Cadence', data.cadence.toStringAsFixed(0),    'rpm'),
          _MC('Torque',  '${data.torque}',                   '')),
    const SizedBox(height: 6),
    _Row3(_MC('Power',   '${data.power}',                    'W'),
          _MC('Current', data.current.toStringAsFixed(1),    'A'),
          _MC('Throttle',data.throttle.toStringAsFixed(2),   'V')),
    const SizedBox(height: 10),
    const Divider(color: Colors.white12),
    const SizedBox(height: 4),
    // Parameter list
    ...params.map((p) => _ParamRow(
        label: p.label, value: p.value,
        active: p.editCase == data.menuState)),
  ]);
}

class _ParamRow extends StatelessWidget {
  final String label, value;
  final bool   active;
  const _ParamRow({required this.label, required this.value, this.active = false});

  @override
  Widget build(BuildContext context) => Container(
    margin: const EdgeInsets.symmetric(vertical: 3),
    padding: const EdgeInsets.symmetric(vertical: 6, horizontal: 10),
    decoration: active ? BoxDecoration(
      color: Colors.red.withOpacity(0.15),
      borderRadius: BorderRadius.circular(6),
      border: Border.all(color: Colors.red.withOpacity(0.6)),
    ) : null,
    child: Row(children: [
      Text(label, style: TextStyle(
          color: active ? Colors.red[300] : Colors.white54, fontSize: 13)),
      const Spacer(),
      Text(value, style: TextStyle(
          color: active ? Colors.red[200] : Colors.white,
          fontSize: 15, fontWeight: FontWeight.bold,
          fontFeatures: const [FontFeature.tabularFigures()])),
    ]),
  );
}

// ── Page 5: DIV PARAMETERS ────────────────────────────────────────────────
class _PageDiv extends StatelessWidget {
  final BikeData data;
  const _PageDiv({required this.data});

  @override
  Widget build(BuildContext context) => Column(children: [
    _ParamRow(label: 'Trip', value: '${data.trip.toStringAsFixed(1)} km'),
    _ParamRow(label: 'Odo',  value: '${data.odo} km'),
    if (data.raceMode) ...[
      _ParamRow(label: 'WheelCirc',          value: '${data.wheelCircle} mm',                           active: data.menuState == 51),
      const Divider(color: Colors.white12, height: 24),
      _ParamRow(label: 'SpeedLimit Street',  value: '${data.speedLimitStreet.toStringAsFixed(1)} km/h', active: data.menuState == 52),
      _ParamRow(label: 'CruiseLimit Street', value: '${data.cruiseLimitStreet.toStringAsFixed(1)} km/h',active: data.menuState == 53),
      _ParamRow(label: 'PowerLimit Street',  value: '${data.powerLimitStreet} W',                       active: data.menuState == 54),
      const SizedBox(height: 8),
      _ParamRow(label: 'SpeedLimit Race',    value: '${data.speedLimitRace.toStringAsFixed(1)} km/h',   active: data.menuState == 55),
      _ParamRow(label: 'CruiseLimit Race',   value: '${data.cruiseLimitRace.toStringAsFixed(1)} km/h',  active: data.menuState == 56),
      _ParamRow(label: 'PowerLimit Race',    value: '${data.powerLimitRace} W',                         active: data.menuState == 57),
    ],
  ]);
}

// ── Page 6: STOPWATCH ─────────────────────────────────────────────────────
class _PageStopwatch extends StatelessWidget {
  final BikeData data;
  const _PageStopwatch({required this.data});

  @override
  Widget build(BuildContext context) {
    return Column(children: [
      _StopwatchCard(data: data),
      const SizedBox(height: 8),
      _ParamRow(label: 'Time',  value: data.rideStr,
          active: data.menuState == 61 || data.menuState == 62),
    ]);
  }
}

// ── Page 7: REAR LIGHT ────────────────────────────────────────────────────
class _PageRearLight extends StatelessWidget {
  final BikeData data;
  const _PageRearLight({required this.data});

  @override
  Widget build(BuildContext context) => Column(children: [
    _ParamRow(label: 'RearLight', value: data.rearLightOnOff == 1 ? 'ON' : 'off'),
    _ParamRow(label: 'Dim',       value: '${data.rearLightDim} %', active: data.menuState == 71),
    const SizedBox(height: 4),
    _BrakingRow(braking: data.braking),
    if (data.raceMode) ...[
      const SizedBox(height: 8),
      _ParamRow(label: 'StreetOnRestart', value: data.streetOnRestart == 1 ? 'YES' : 'NO', active: data.menuState == 72),
    ],
  ]);
}

// ── Page 8: FRONT LIGHT ───────────────────────────────────────────────────
class _PageFrontLight extends StatelessWidget {
  final BikeData data;
  const _PageFrontLight({required this.data});

  @override
  Widget build(BuildContext context) => Column(children: [
    _ParamRow(label: 'FrontLight', value: data.frontLightOnOff == 1 ? 'ON' : 'off'),
    _ParamRow(label: 'Dim',        value: '${data.frontLightDim} %',  active: data.menuState == 81),
    const SizedBox(height: 8),
    _ParamRow(label: 'Backlight',  value: data.backlightStr,          active: data.menuState == 82),
    _ParamRow(label: 'OnTime',     value: data.onTimeBlStr,           active: data.menuState == 83),
  ]);
}

// ── Shared widgets ────────────────────────────────────────────────────────
class _SpeedCard extends StatelessWidget {
  final double speed;
  final bool   raceMode;
  const _SpeedCard({required this.speed, required this.raceMode});

  @override
  Widget build(BuildContext context) => Container(
    width: double.infinity,
    padding: const EdgeInsets.symmetric(vertical: 14, horizontal: 16),
    decoration: BoxDecoration(
      color: const Color(0xFF16213E), borderRadius: BorderRadius.circular(10),
      border: Border.all(color: raceMode ? Colors.red.withOpacity(0.4) : Colors.white12),
    ),
    child: Row(
      crossAxisAlignment: CrossAxisAlignment.baseline,
      textBaseline: TextBaseline.alphabetic,
      children: [
        const Text('Speed', style: TextStyle(color: Colors.white54, fontSize: 13)),
        const Spacer(),
        Text(speed.toStringAsFixed(1),
            style: const TextStyle(color: Colors.white, fontSize: 52,
                fontWeight: FontWeight.bold,
                fontFeatures: [FontFeature.tabularFigures()])),
        const SizedBox(width: 6),
        Text('km/h', style: TextStyle(
            color: raceMode ? Colors.red : Colors.white38, fontSize: 22)),
      ],
    ),
  );
}

class _StopwatchCard extends StatelessWidget {
  final BikeData data;
  const _StopwatchCard({required this.data});

  @override
  Widget build(BuildContext context) => Container(
    width: double.infinity,
    padding: const EdgeInsets.symmetric(vertical: 10, horizontal: 16),
    decoration: BoxDecoration(
      color: const Color(0xFF16213E), borderRadius: BorderRadius.circular(10),
      border: Border.all(
          color: data.stopwatchState == 1 ? Colors.greenAccent.withOpacity(0.4) : Colors.white12),
    ),
    child: Row(children: [
      const Text('Stopwatch', style: TextStyle(color: Colors.white54, fontSize: 13)),
      const Spacer(),
      Text(data.stopwatchStr,
          style: TextStyle(
            color: data.stopwatchState == 1 ? Colors.greenAccent : Colors.white,
            fontSize: 22, fontWeight: FontWeight.bold,
            fontFeatures: const [FontFeature.tabularFigures()],
          )),
    ]),
  );
}

class _Row3 extends StatelessWidget {
  final Widget a, b, c;
  const _Row3(this.a, this.b, this.c);

  @override
  Widget build(BuildContext context) => Row(children: [
    Expanded(child: a), const SizedBox(width: 6),
    Expanded(child: b), const SizedBox(width: 6),
    Expanded(child: c),
  ]);
}

class _BrakingRow extends StatelessWidget {
  final bool braking;
  const _BrakingRow({required this.braking});

  @override
  Widget build(BuildContext context) => Container(
    margin: const EdgeInsets.symmetric(vertical: 3),
    padding: const EdgeInsets.symmetric(vertical: 6, horizontal: 10),
    decoration: braking ? BoxDecoration(
      color: Colors.red.withOpacity(0.15),
      borderRadius: BorderRadius.circular(6),
      border: Border.all(color: Colors.red.withOpacity(0.6)),
    ) : null,
    child: Row(children: [
      Text('Braking', style: TextStyle(
          color: braking ? Colors.red[300] : Colors.white54, fontSize: 13)),
      const Spacer(),
      Text(braking ? 'YES' : 'NO', style: TextStyle(
          color: braking ? Colors.red[200] : Colors.white54,
          fontSize: 15, fontWeight: FontWeight.bold)),
    ]),
  );
}

class _MC extends StatelessWidget {
  final String label, value, unit;
  const _MC(this.label, this.value, this.unit);

  @override
  Widget build(BuildContext context) => Container(
    padding: const EdgeInsets.fromLTRB(10, 8, 10, 10),
    decoration: BoxDecoration(
      color: const Color(0xFF16213E), borderRadius: BorderRadius.circular(8),
      border: Border.all(color: Colors.white12),
    ),
    child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      Text(label, style: const TextStyle(color: Colors.white54, fontSize: 11)),
      const SizedBox(height: 4),
      RichText(text: TextSpan(children: [
        TextSpan(text: value, style: const TextStyle(color: Colors.white, fontSize: 20,
            fontWeight: FontWeight.bold,
            fontFeatures: [FontFeature.tabularFigures()])),
        if (unit.isNotEmpty)
          TextSpan(text: ' $unit',
              style: const TextStyle(color: Colors.white38, fontSize: 13)),
      ])),
    ]),
  );
}
