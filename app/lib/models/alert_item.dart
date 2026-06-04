import 'package:cloud_firestore/cloud_firestore.dart';

class AlertItem {
  final String id;
  final String type;       // 'critical' or 'warning'
  final String parameter;  // 'level', 'pH', 'turbidity', 'leak'
  final double value;
  final String threshold;
  final String message;
  final DateTime timestamp;
  final bool resolved;

  AlertItem({
    required this.id,
    required this.type,
    required this.parameter,
    required this.value,
    required this.threshold,
    required this.message,
    required this.timestamp,
    required this.resolved,
  });

  factory AlertItem.fromFirestore(String id, Map<String, dynamic> data) {
    return AlertItem(
      id:        id,
      type:      data['type']      ?? 'warning',
      parameter: data['parameter'] ?? '',
      value:     (data['value']    ?? 0.0).toDouble(),
      threshold: data['threshold'].toString(),
      message:   data['message']   ?? '',
      timestamp: (data['timestamp'] as Timestamp?)?.toDate() ?? DateTime.now(),
      resolved:  data['resolved']  ?? false,
    );
  }

  String get parameterIcon {
    switch (parameter) {
      case 'level':     return '💧';
      case 'pH':        return '🧪';
      case 'turbidity': return '🌊';
      case 'leak':      return '⚠️';
      case 'offline':   return '📡';
      default:          return '❗';
    }
  }
}