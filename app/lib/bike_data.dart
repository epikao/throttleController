import 'dart:convert';

class BikeData {
  // Live values
  final double speed, cadence, current, voltage, throttle, temp;
  final double trip, whKm, speedAvg;
  final int    torque, power, odo, range, capacity, onTimeSec, rideSec;
  final bool   raceMode, cruise;
  final int    support, page, menuState;
  final int    stopwatchCs, stopwatchState;
  // Diagnostic
  final double diagRawNorm, diagFilteredTorque, diagTarget;
  final int    diagCadenceGateOpen;
  // Torque parameters
  final int    torqueZero, torqueMax, torqueDeadband, torqueIdleMs;
  final double torqueFilterRise, torqueFilterFall;
  // Cadence parameters
  final int    pulsesPerRev, cadenceTimeoutMs;
  final int    cadenceGatePulses, cadenceGateMs;
  final double cadenceMaxRpm, cadenceFilterAlpha, cadenceMaxLimit;
  // Curve parameters
  final double curveY25, curveY50, curveY75, curveOffset;
  final double rampUpLow, rampUpHigh, rampThreshold, rampDown;
  // Cruise & Battery parameters
  final int    cruisePower, pasMode;
  final double cruiseRampUp, cruiseRampDown, cruiseDeadband;
  final double voltageMin, voltageMax, currentMax, capacityMax;
  // Div parameters
  final int    wheelCircle, powerLimitStreet, powerLimitRace;
  final double speedLimitStreet, cruiseLimitStreet;
  final double speedLimitRace,   cruiseLimitRace;
  // Light parameters
  final int    rearLightOnOff, rearLightDim, streetOnRestart;
  final int    frontLightOnOff, frontLightDim, backlightDim, onTimeBacklight;
  final bool   braking;

  const BikeData({
    this.speed = 0, this.cadence = 0, this.current = 0, this.voltage = 0,
    this.throttle = 0, this.temp = 0, this.trip = 0, this.whKm = 0,
    this.speedAvg = 0, this.torque = 0, this.power = 0, this.odo = 0,
    this.range = 0, this.capacity = 0, this.onTimeSec = 0, this.rideSec = 0,
    this.raceMode = false, this.cruise = false, this.support = 0, this.page = 0,
    this.menuState = 0, this.stopwatchCs = 0, this.stopwatchState = 0,
    this.diagRawNorm = 0, this.diagFilteredTorque = 0, this.diagTarget = 0,
    this.diagCadenceGateOpen = 0,
    this.torqueZero = 0, this.torqueMax = 0, this.torqueDeadband = 0,
    this.torqueIdleMs = 0,
    this.torqueFilterRise = 0, this.torqueFilterFall = 0,
    this.pulsesPerRev = 0, this.cadenceTimeoutMs = 0,
    this.cadenceGatePulses = 0, this.cadenceGateMs = 0,
    this.cadenceMaxRpm = 0, this.cadenceFilterAlpha = 0, this.cadenceMaxLimit = 0,
    this.curveY25 = 0, this.curveY50 = 0, this.curveY75 = 0, this.curveOffset = 0,
    this.rampUpLow = 0, this.rampUpHigh = 0, this.rampThreshold = 0, this.rampDown = 0,
    this.cruisePower = 0, this.pasMode = 0,
    this.cruiseRampUp = 0, this.cruiseRampDown = 0, this.cruiseDeadband = 0,
    this.voltageMin = 0, this.voltageMax = 0, this.currentMax = 0, this.capacityMax = 0,
    this.wheelCircle = 0, this.powerLimitStreet = 0, this.powerLimitRace = 0,
    this.speedLimitStreet = 0, this.cruiseLimitStreet = 0,
    this.speedLimitRace = 0, this.cruiseLimitRace = 0,
    this.rearLightOnOff = 0, this.rearLightDim = 0, this.streetOnRestart = 0,
    this.frontLightOnOff = 0, this.frontLightDim = 0,
    this.backlightDim = 0, this.onTimeBacklight = 0,
    this.braking = false,
  });

  static double _d(dynamic v) => (v as num?)?.toDouble() ?? 0.0;
  static int    _i(dynamic v) => (v as num?)?.toInt()    ?? 0;

  factory BikeData.fromJson(Map<String, dynamic> j) => BikeData(
    speed: _d(j['sp']), cadence: _d(j['cd']), current: _d(j['cu']),
    voltage: _d(j['vo']), throttle: _d(j['th']), temp: _d(j['tp']),
    trip: _d(j['tr']), whKm: _d(j['wh']), speedAvg: _d(j['sa']),
    torque: _i(j['tq']), power: _i(j['pw']), odo: _i(j['od']),
    range: _i(j['ra']), capacity: _i(j['ca']),
    onTimeSec: _i(j['ot']), rideSec: _i(j['t']),
    raceMode: _i(j['rc']) == 1, cruise: _i(j['crs']) == 1,
    support: _i(j['su']), page: _i(j['pg']), menuState: _i(j['ms']),
    stopwatchCs: _i(j['sw']), stopwatchState: _i(j['ss']),
    diagRawNorm: _d(j['drn']), diagFilteredTorque: _d(j['dft']),
    diagCadenceGateOpen: _i(j['dgt']), diagTarget: _d(j['dtg']),
    torqueZero: _i(j['tz']), torqueMax: _i(j['tm']), torqueDeadband: _i(j['tdb']),
    torqueIdleMs: _i(j['tidl']),
    torqueFilterRise: _d(j['tfr']), torqueFilterFall: _d(j['tff']),
    pulsesPerRev: _i(j['ppr']), cadenceTimeoutMs: _i(j['cto']),
    cadenceGatePulses: _i(j['cgp']), cadenceGateMs: _i(j['cgt']),
    cadenceMaxRpm: _d(j['crpm']), cadenceFilterAlpha: _d(j['cfa']), cadenceMaxLimit: _d(j['cml']),
    curveY25: _d(j['cy25']), curveY50: _d(j['cy50']), curveY75: _d(j['cy75']),
    curveOffset: _d(j['cyo']),
    rampUpLow: _d(j['rul']), rampUpHigh: _d(j['ruh']),
    rampThreshold: _d(j['rth']), rampDown: _d(j['rdn']),
    cruisePower: _i(j['cp']), pasMode: _i(j['pmd']),
    cruiseRampUp: _d(j['cru']), cruiseRampDown: _d(j['crd']),
    cruiseDeadband: _d(j['cdb']),
    voltageMin: _d(j['vmn']), voltageMax: _d(j['vmx']),
    currentMax: _d(j['imx']), capacityMax: _d(j['cpmx']),
    wheelCircle: _i(j['wc']), powerLimitStreet: _i(j['pls']), powerLimitRace: _i(j['plr']),
    speedLimitStreet: _d(j['sls']), cruiseLimitStreet: _d(j['cls']),
    speedLimitRace: _d(j['slr']), cruiseLimitRace: _d(j['clr']),
    rearLightOnOff: _i(j['rlon']), rearLightDim: _i(j['rld']),
    streetOnRestart: _i(j['sor']),
    frontLightOnOff: _i(j['flon']), frontLightDim: _i(j['fld']),
    backlightDim: _i(j['bld']), onTimeBacklight: _i(j['otb']),
    braking: _i(j['brk']) == 1,
  );

  static BikeData? tryParse(String line) {
    try {
      final j = jsonDecode(line) as Map<String, dynamic>;
      if (!j.containsKey('sp')) return null;
      return BikeData.fromJson(j);
    } catch (_) { return null; }
  }

  static String _hms(int sec) =>
      '${(sec ~/ 3600).toString().padLeft(2, '0')}:'
      '${((sec % 3600) ~/ 60).toString().padLeft(2, '0')}:'
      '${(sec % 60).toString().padLeft(2, '0')}';

  String get onTimeStr => _hms(onTimeSec);
  String get rideStr   => _hms(rideSec);

  String get stopwatchStr =>
      '${(stopwatchCs ~/ 360000).toString().padLeft(2, '0')}:'
      '${((stopwatchCs ~/ 6000) % 60).toString().padLeft(2, '0')}:'
      '${((stopwatchCs ~/ 100) % 60).toString().padLeft(2, '0')}.'
      '${(stopwatchCs % 100).toString().padLeft(2, '0')}';

  String get backlightStr  => backlightDim  == 101  ? 'AUTO' : '$backlightDim %';
  String get onTimeBlStr   => onTimeBacklight == 1000 ? 'ON'   : '$onTimeBacklight sec';
}
