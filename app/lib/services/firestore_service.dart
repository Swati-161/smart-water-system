import 'package:cloud_firestore/cloud_firestore.dart';
import '../models/sensor_data.dart';
import '../models/alert_item.dart';

class FirestoreService {
  final _db       = FirebaseFirestore.instance;
  final _deviceId = 'node_01';

  // ── Live dashboard stream ──────────────────────────────────
  // Updates within 200ms of new data arriving
  Stream<SensorData> latestReadingStream() {
    return _db.collection('devices').doc(_deviceId)
        .snapshots()
        .map((doc) {
          final data = doc.data()?['latest'] as Map<String, dynamic>? ?? {};
          return SensorData.fromFirestore(data);
        });
  }

  // ── Device online/offline status ──────────────────────────
  Stream<String> deviceStatusStream() {
    return _db.collection('devices').doc(_deviceId)
        .snapshots()
        .map((doc) => doc.data()?['status'] as String? ?? 'unknown');
  }

  // ── History for graphs ─────────────────────────────────────
  Future<List<SensorData>> getHistory(int days) async {
    final since = DateTime.now().subtract(Duration(days: days));
    final snap  = await _db
        .collection('devices').doc(_deviceId)
        .collection('readings')
        .where('timestamp', isGreaterThan: Timestamp.fromDate(since))
        .orderBy('timestamp', descending: false)
        .limit(500) // cap to prevent huge reads
        .get();

    return snap.docs
        .map((d) => SensorData.fromFirestore(d.data()))
        .toList();
  }

  // ── Daily consumption ─────────────────────────────────────
  Future<List<Map<String, dynamic>>> getDailyConsumption(int days) async {
    final history = await getHistory(days);
    final Map<String, double> dailyMap = {};

    for (final reading in history) {
      final dateKey = '${reading.timestamp.day}/${reading.timestamp.month}';
      dailyMap[dateKey] = (dailyMap[dateKey] ?? 0) + 
          (reading.totalLitres / 48); // approximate per reading
    }

    return dailyMap.entries
        .map((e) => {'date': e.key, 'litres': e.value})
        .toList();
  }

  // ── Alerts ────────────────────────────────────────────────
  Stream<List<AlertItem>> alertsStream() {
    return _db
        .collection('devices').doc(_deviceId)
        .collection('alerts')
        .orderBy('timestamp', descending: true)
        .limit(50)
        .snapshots()
        .map((snap) => snap.docs
            .map((d) => AlertItem.fromFirestore(d.id, d.data()))
            .toList());
  }

  // ── Mark alert as resolved ────────────────────────────────
  Future<void> resolveAlert(String alertId) async {
    await _db
        .collection('devices').doc(_deviceId)
        .collection('alerts').doc(alertId)
        .update({'resolved': true});
  }

  // ── Save threshold config ─────────────────────────────────
  Future<void> saveThresholds(Map<String, double> thresholds) async {
    await _db.collection('devices').doc(_deviceId)
        .set({'thresholds': thresholds}, SetOptions(merge: true));
  }

  // ── Get threshold config ──────────────────────────────────
  Future<Map<String, dynamic>> getThresholds() async {
    final doc = await _db.collection('devices').doc(_deviceId).get();
    return doc.data()?['thresholds'] as Map<String, dynamic>? ?? {};
  }
}