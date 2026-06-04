class SensorData {
  final double levelPct;
  final double ph;
  final double turbidityNtu;
  final double flowLpm;
  final double totalLitres;
  final bool leakDetected;
  final double batteryV;
  final bool valveOpen;
  final bool mock;
  final DateTime timestamp;

  SensorData({
    required this.levelPct,
    required this.ph,
    required this.turbidityNtu,
    required this.flowLpm,
    required this.totalLitres,
    required this.leakDetected,
    required this.batteryV,
    required this.valveOpen,
    required this.mock,
    required this.timestamp,
  });

  // Parse from Firestore document
  factory SensorData.fromFirestore(Map<String, dynamic> data) {
    return SensorData(
      levelPct:      (data['level_pct']      ?? 0.0).toDouble(),
      ph:            (data['ph']              ?? 7.0).toDouble(),
      turbidityNtu:  (data['turbidity_ntu']   ?? 0.0).toDouble(),
      flowLpm:       (data['flow_lpm']        ?? 0.0).toDouble(),
      totalLitres:   (data['total_litres']    ?? 0.0).toDouble(),
      leakDetected:  data['leak_detected']    ?? false,
      batteryV:      (data['battery_v']       ?? 0.0).toDouble(),
      valveOpen:     data['valve_open']       ?? false,
      mock:          data['mock']             ?? false,
      timestamp:     (data['timestamp'] as dynamic)?.toDate() ?? DateTime.now(),
    );
  }
}