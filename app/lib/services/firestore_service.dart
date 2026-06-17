// lib/services/firestore_service.dart
import 'package:cloud_firestore/cloud_firestore.dart';
import '../models/sensor_data.dart';
import '../models/alert_item.dart';

class FirestoreService {
  final _db       = FirebaseFirestore.instance;
  final _deviceId = 'node_01';

  // ── Live dashboard stream ─────────────────────────────────
  Stream<SensorData> latestReadingStream() {
    return _db.collection('devices').doc(_deviceId)
        .snapshots()
        .map((doc) {
          final data = doc.data()?['latest'] as Map<String, dynamic>? ?? {};
          return SensorData.fromFirestore(data);
        });
  }

  // ── Device online/offline ─────────────────────────────────
  Stream<String> deviceStatusStream() {
    return _db.collection('devices').doc(_deviceId)
        .snapshots()
        .map((doc) => doc.data()?['status'] as String? ?? 'unknown');
  }

  // ── History for graphs ────────────────────────────────────
  Future<List<SensorData>> getHistory(int days) async {
    final since = DateTime.now().subtract(Duration(days: days));
    final snap  = await _db
        .collection('devices').doc(_deviceId)
        .collection('readings')
        .where('timestamp', isGreaterThan: Timestamp.fromDate(since))
        .orderBy('timestamp', descending: false)
        .limit(500)
        .get();

    return snap.docs
        .map((d) => SensorData.fromFirestore(d.data()))
        .toList();
  }

  // ── Daily consumption from backend-computed collection ────
  // Reads from daily_consumption subcollection written by server.js
  Future<List<Map<String, dynamic>>> getDailyConsumption() async {
    final snap = await _db
        .collection('devices').doc(_deviceId)
        .collection('daily_consumption')
        .orderBy('date', descending: true)
        .limit(7)
        .get();

    return snap.docs.map((d) => d.data()).toList();
  }

  // ── Alerts stream ─────────────────────────────────────────
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

  // ── Resolve alert ─────────────────────────────────────────
  Future<void> resolveAlert(String alertId) async {
    await _db
        .collection('devices').doc(_deviceId)
        .collection('alerts').doc(alertId)
        .update({'resolved': true});
  }

  // ── Valve command via Firestore ───────────────────────────
  // Writes a command doc that server.js picks up and forwards to MQTT
  Future<void> sendValveCommand(String action) async {
    await _db.collection('devices').doc(_deviceId)
        .collection('commands').add({
          'action':    action,   // 'OPEN' or 'CLOSE'
          'timestamp': FieldValue.serverTimestamp(),
          'source':    'app',
          'executed':  false,
        });
  }

  // ── Save thresholds ───────────────────────────────────────
  Future<void> saveThresholds(Map<String, double> thresholds) async {
    await _db.collection('devices').doc(_deviceId)
        .set({'thresholds': thresholds}, SetOptions(merge: true));
  }

  // ── Get thresholds ────────────────────────────────────────
  Future<Map<String, dynamic>> getThresholds() async {
    final doc = await _db.collection('devices').doc(_deviceId).get();
    return doc.data()?['thresholds'] as Map<String, dynamic>? ?? {};
  }
}