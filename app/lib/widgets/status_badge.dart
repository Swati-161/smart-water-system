import 'package:flutter/material.dart';
import '../utils/constants.dart';

class StatusBadge extends StatelessWidget {
  final String status; // 'online', 'offline', 'unknown'
  final DateTime? lastSeen;

  const StatusBadge({super.key, required this.status, this.lastSeen});

  @override
  Widget build(BuildContext context) {
    final isOnline = status == 'online';
    final color    = isOnline ? AppColors.safe : AppColors.danger;
    final label    = isOnline ? 'Online' : 'Offline';

    return Row(mainAxisSize: MainAxisSize.min, children: [
      Container(
        width: 8, height: 8,
        decoration: BoxDecoration(
          color: color,
          shape: BoxShape.circle,
        ),
      ),
      const SizedBox(width: 6),
      Text(label,
        style: TextStyle(
          color: color,
          fontSize: 12,
          fontWeight: FontWeight.w600,
        )),
      if (lastSeen != null) ...[
        const SizedBox(width: 4),
        Text('· ${_timeAgo(lastSeen!)}',
          style: TextStyle(fontSize: 11, color: Colors.grey[500])),
      ],
    ]);
  }

  String _timeAgo(DateTime t) {
    final diff = DateTime.now().difference(t);
    if (diff.inSeconds < 60)  return '${diff.inSeconds}s ago';
    if (diff.inMinutes < 60)  return '${diff.inMinutes}m ago';
    if (diff.inHours < 24)    return '${diff.inHours}h ago';
    return '${diff.inDays}d ago';
  }
}