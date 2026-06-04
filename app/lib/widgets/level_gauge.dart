import 'package:flutter/material.dart';
import 'package:percent_indicator/percent_indicator.dart';
import '../utils/constants.dart';

class LevelGauge extends StatelessWidget {
  final double percentage;

  const LevelGauge({super.key, required this.percentage});

  Color get _color {
    if (percentage >= AppThresholds.levelHigh)   return AppColors.danger;
    if (percentage <= AppThresholds.levelCritLow) return AppColors.danger;
    if (percentage <= AppThresholds.levelWarnLow) return AppColors.warning;
    return AppColors.safe;
  }

  String get _label {
    if (percentage >= AppThresholds.levelHigh)    return 'FULL — Auto cutoff';
    if (percentage <= AppThresholds.levelCritLow) return 'CRITICAL LOW';
    if (percentage <= AppThresholds.levelWarnLow) return 'LOW — Refill soon';
    return 'Normal';
  }

  @override
  Widget build(BuildContext context) {
    final pct = percentage.clamp(0.0, 100.0) / 100.0;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(children: [
          Text('Water Level',
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
              fontWeight: FontWeight.bold,
            )),
          const SizedBox(height: 16),
          CircularPercentIndicator(
            radius:          80,
            lineWidth:       14,
            percent:         pct,
            center:          Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Text('${percentage.toStringAsFixed(1)}%',
                  style: TextStyle(
                    fontSize: 26,
                    fontWeight: FontWeight.bold,
                    color: _color,
                  )),
                Text('level',
                  style: TextStyle(fontSize: 12, color: Colors.grey[600])),
              ],
            ),
            progressColor:   _color,
            backgroundColor: Colors.grey[200]!,
            circularStrokeCap: CircularStrokeCap.round,
          ),
          const SizedBox(height: 12),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
            decoration: BoxDecoration(
              color: _color.withOpacity(0.1),
              borderRadius: BorderRadius.circular(20),
            ),
            child: Text(_label,
              style: TextStyle(
                color: _color,
                fontWeight: FontWeight.w600,
                fontSize: 13,
              )),
          ),
        ]),
      ),
    );
  }
}