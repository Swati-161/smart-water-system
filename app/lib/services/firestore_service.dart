// lib/services/firestore_service.dart
import 'package:cloud_firestore/cloud_firestore.dart';
import '../models/sensor_data.dart';

class FirestoreService {
  final _db = FirebaseFirestore.instance;
  final String deviceId = 'node_01';

  // Live stream for dashboard — updates in real time
  Stream<SensorData> latestReadingStream() {
    return _db
        .collection('devices')
        .doc(deviceId)
        .snapshots()
        .map((doc) {
          final data = doc.data()?['latest'] as Map<String, dynamic>? ?? {};
          return SensorData.fromFirestore(data);
        });
  }

  // History for graphs
  Future<List<SensorData>> getHistory(int days) async {
    final since = DateTime.now().subtract(Duration(days: days));
    final snapshot = await _db
        .collection('devices')
        .doc(deviceId)
        .collection('readings')
        .where('timestamp', isGreaterThan: Timestamp.fromDate(since))
        .orderBy('timestamp', descending: false)
        .get();

    return snapshot.docs
        .map((doc) => SensorData.fromFirestore(doc.data()))
        .toList();
  }

  // Device online/offline status
  Stream<String> deviceStatusStream() {
    return _db
        .collection('devices')
        .doc(deviceId)
        .snapshots()
        .map((doc) => doc.data()?['status'] ?? 'unknown');
  }
}