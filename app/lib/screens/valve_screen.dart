// lib/screens/valve_screen.dart
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/firestore_service.dart';
import '../models/sensor_data.dart';
import '../utils/constants.dart';

class ValveScreen extends StatelessWidget {
  const ValveScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final service = context.read<FirestoreService>();

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: const Text('Valve Control'),
        backgroundColor: AppColors.primary,
        foregroundColor: Colors.white,
      ),
      body: StreamBuilder<SensorData>(
        stream: service.latestReadingStream(),
        builder: (context, snap) {
          final valveOpen = snap.data?.valveOpen ?? false;
          final level     = snap.data?.levelPct  ?? 0.0;

          return SingleChildScrollView(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [

                // Current valve status card
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(20),
                    child: Column(children: [
                      Icon(
                        valveOpen ? Icons.water : Icons.water_drop_outlined,
                        size: 48,
                        color: valveOpen ? AppColors.safe : AppColors.danger,
                      ),
                      const SizedBox(height: 8),
                      Text(
                        valveOpen ? 'VALVE OPEN' : 'VALVE CLOSED',
                        style: TextStyle(
                          fontSize: 22, fontWeight: FontWeight.bold,
                          color: valveOpen ? AppColors.safe : AppColors.danger,
                        ),
                      ),
                      Text('Water supply is ${valveOpen ? "flowing" : "stopped"}',
                        style: TextStyle(fontSize: 13, color: Colors.grey[600])),
                    ]),
                  ),
                ),
                const SizedBox(height: 16),

                // Water level reminder
                Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: level >= AppThresholds.levelHigh
                        ? AppColors.danger.withOpacity(0.08)
                        : AppColors.primary.withOpacity(0.06),
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(
                      color: level >= AppThresholds.levelHigh
                          ? AppColors.danger.withOpacity(0.3)
                          : AppColors.primary.withOpacity(0.2)),
                  ),
                  child: Row(children: [
                    Icon(Icons.water_drop, size: 16,
                      color: level >= AppThresholds.levelHigh
                          ? AppColors.danger : AppColors.primary),
                    const SizedBox(width: 8),
                    Text('Current tank level: ${level.toStringAsFixed(1)}%',
                      style: TextStyle(fontSize: 13,
                        color: level >= AppThresholds.levelHigh
                            ? AppColors.danger : AppColors.primary,
                        fontWeight: FontWeight.w500)),
                  ]),
                ),
                const SizedBox(height: 24),

                // Manual control buttons
                Text('Manual Control',
                  style: TextStyle(
                    fontSize: 14, fontWeight: FontWeight.w600,
                    color: Colors.grey[700])),
                const SizedBox(height: 12),

                Row(children: [
                  Expanded(
                    child: _ValveButton(
                      label: 'Open Valve',
                      icon: Icons.water,
                      color: AppColors.safe,
                      enabled: !valveOpen,
                      onPressed: () => _confirm(context, 'OPEN',
                        'Open the valve and allow water to flow?', service),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: _ValveButton(
                      label: 'Close Valve',
                      icon: Icons.water_drop_outlined,
                      color: AppColors.danger,
                      enabled: valveOpen,
                      onPressed: () => _confirm(context, 'CLOSE',
                        'Close the valve and stop water flow?', service),
                    ),
                  ),
                ]),
                const SizedBox(height: 28),

                // Auto-cutoff info
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(children: [
                          Icon(Icons.auto_mode,
                            size: 18, color: AppColors.primary),
                          const SizedBox(width: 8),
                          const Text('Automatic Behaviour',
                            style: TextStyle(
                              fontWeight: FontWeight.w600, fontSize: 14)),
                        ]),
                        const SizedBox(height: 10),
                        _InfoRow(Icons.arrow_upward,
                          'Tank > ${AppThresholds.levelHigh.toInt()}%',
                          'Valve closes automatically'),
                        _InfoRow(Icons.warning_amber,
                          'Leak detected',
                          'Alert sent — valve stays as-is'),
                        _InfoRow(Icons.wifi_off,
                          'Device offline',
                          'Valve stays in last known state'),
                        _InfoRow(Icons.power_off,
                          'Power failure',
                          'Normally-closed valve closes safely'),
                      ],
                    ),
                  ),
                ),
              ],
            ),
          );
        },
      ),
    );
  }

  Future<void> _confirm(BuildContext context, String action,
      String message, FirestoreService service) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        title: Text('${action == "OPEN" ? "Open" : "Close"} Valve'),
        content: Text(message),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel')),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            style: FilledButton.styleFrom(
              backgroundColor: action == 'OPEN'
                  ? AppColors.safe : AppColors.danger),
            child: Text(action == 'OPEN' ? 'Open' : 'Close')),
        ],
      ),
    );
    if (ok == true) {
      await service.sendValveCommand(action);
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Valve ${action.toLowerCase()} command sent'),
            backgroundColor: action == 'OPEN' ? AppColors.safe : AppColors.danger,
            duration: const Duration(seconds: 2),
          ),
        );
      }
    }
  }
}

class _ValveButton extends StatelessWidget {
  final String label;
  final IconData icon;
  final Color color;
  final bool enabled;
  final VoidCallback onPressed;

  const _ValveButton({
    required this.label, required this.icon,
    required this.color, required this.enabled,
    required this.onPressed,
  });

  @override
  Widget build(BuildContext context) {
    return ElevatedButton.icon(
      onPressed: enabled ? onPressed : null,
      icon: Icon(icon),
      label: Text(label),
      style: ElevatedButton.styleFrom(
        backgroundColor: color,
        foregroundColor: Colors.white,
        disabledBackgroundColor: Colors.grey[200],
        disabledForegroundColor: Colors.grey[400],
        padding: const EdgeInsets.symmetric(vertical: 14),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(10)),
      ),
    );
  }
}

class _InfoRow extends StatelessWidget {
  final IconData icon;
  final String trigger, action;
  const _InfoRow(this.icon, this.trigger, this.action);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(children: [
        Icon(icon, size: 14, color: Colors.grey[500]),
        const SizedBox(width: 8),
        Text(trigger,
          style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w500)),
        const SizedBox(width: 4),
        Text('→', style: TextStyle(color: Colors.grey[400])),
        const SizedBox(width: 4),
        Expanded(
          child: Text(action,
            style: TextStyle(fontSize: 12, color: Colors.grey[600]))),
      ]),
    );
  }
}