import 'package:flutter/material.dart';
import '../utils/constants.dart';

class SensorCard extends StatelessWidget {
  final String label;
  final String value;
  final IconData icon;
  final Color? color;
  final String? subtitle;

  const SensorCard({
    super.key,
    required this.label,
    required this.value,
    required this.icon,
    this.color,
    this.subtitle,
  });

  @override
  Widget build(BuildContext context) {
    final cardColor = color ?? AppColors.primary;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(children: [
              Icon(icon, color: cardColor, size: 20),
              const SizedBox(width: 6),
              Text(label,
                style: TextStyle(
                  fontSize: 12,
                  color: Colors.grey[600],
                  fontWeight: FontWeight.w500,
                )),
            ]),
            const SizedBox(height: 8),
            Text(value,
              style: TextStyle(
                fontSize: 22,
                fontWeight: FontWeight.bold,
                color: cardColor,
              )),
            if (subtitle != null) ...[
              const SizedBox(height: 2),
              Text(subtitle!,
                style: TextStyle(fontSize: 11, color: Colors.grey[500])),
            ],
          ],
        ),
      ),
    );
  }
}