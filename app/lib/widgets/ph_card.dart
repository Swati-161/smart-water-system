import 'package:flutter/material.dart';
import '../utils/constants.dart';

class PHCard extends StatelessWidget {
  final double pH;
  const PHCard({super.key, required this.pH});

  Color get _color {
    if (pH < AppThresholds.phCritLow || pH > AppThresholds.phCritHigh)
      return AppColors.danger;
    if (pH < AppThresholds.phWarnLow || pH > AppThresholds.phWarnHigh)
      return AppColors.warning;
    return AppColors.safe;
  }

  String get _label {
    if (pH < AppThresholds.phCritLow)  return 'Too Acidic';
    if (pH > AppThresholds.phCritHigh) return 'Too Alkaline';
    if (pH < AppThresholds.phWarnLow || pH > AppThresholds.phWarnHigh)
      return 'Monitor';
    return 'Safe';
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(children: [
              Icon(Icons.science_outlined, color: _color, size: 20),
              const SizedBox(width: 6),
              Text('pH Level',
                style: TextStyle(fontSize: 12, color: Colors.grey[600])),
            ]),
            const SizedBox(height: 8),
            Text(pH >= 0 ? pH.toStringAsFixed(2) : '--',
              style: TextStyle(
                fontSize: 28,
                fontWeight: FontWeight.bold,
                color: _color,
              )),
            const SizedBox(height: 4),
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
              decoration: BoxDecoration(
                color: _color.withOpacity(0.1),
                borderRadius: BorderRadius.circular(12),
              ),
              child: Text(_label,
                style: TextStyle(
                  color: _color,
                  fontSize: 11,
                  fontWeight: FontWeight.w600,
                )),
            ),
            const SizedBox(height: 4),
            Text('Safe: 6.5 – 8.5 (BIS)',
              style: TextStyle(fontSize: 10, color: Colors.grey[500])),
          ],
        ),
      ),
    );
  }
}