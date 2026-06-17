// lib/screens/alerts_screen.dart
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:intl/intl.dart';
import '../services/firestore_service.dart';
import '../models/alert_item.dart';
import '../utils/constants.dart';

class AlertsScreen extends StatefulWidget {
  const AlertsScreen({super.key});
  @override
  State<AlertsScreen> createState() => _AlertsScreenState();
}

class _AlertsScreenState extends State<AlertsScreen> {
  String _filter = 'All';
  final _filters = ['All', 'level', 'pH', 'turbidity', 'leak', 'offline', 'battery'];

  @override
  Widget build(BuildContext context) {
    final service = context.read<FirestoreService>();

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: const Text('Alert History'),
        backgroundColor: AppColors.primary,
        foregroundColor: Colors.white,
      ),
      body: Column(
        children: [
          // Filter chips
          SizedBox(
            height: 48,
            child: ListView.separated(
              scrollDirection: Axis.horizontal,
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
              itemCount: _filters.length,
              separatorBuilder: (_, __) => const SizedBox(width: 8),
              itemBuilder: (_, i) {
                final f = _filters[i];
                final selected = _filter == f;
                return FilterChip(
                  label: Text(f,
                    style: TextStyle(
                      fontSize: 12,
                      color: selected ? Colors.white : Colors.grey[700])),
                  selected: selected,
                  selectedColor: AppColors.primary,
                  backgroundColor: Colors.white,
                  onSelected: (_) => setState(() => _filter = f),
                  side: BorderSide(
                    color: selected ? AppColors.primary : Colors.grey[300]!),
                );
              },
            ),
          ),
          // Alerts list
          Expanded(
            child: StreamBuilder<List<AlertItem>>(
              stream: service.alertsStream(),
              builder: (context, snap) {
                if (!snap.hasData) {
                  return const Center(child: CircularProgressIndicator());
                }
                var alerts = snap.data!;
                if (_filter != 'All') {
                  alerts = alerts.where((a) => a.parameter == _filter).toList();
                }
                if (alerts.isEmpty) {
                  return Center(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(Icons.check_circle_outline,
                          size: 56, color: AppColors.safe),
                        const SizedBox(height: 12),
                        Text(_filter == 'All'
                          ? 'No alerts yet — system is running normally'
                          : 'No $_filter alerts',
                          style: TextStyle(color: Colors.grey[600])),
                      ],
                    ),
                  );
                }
                return ListView.builder(
                  padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                  itemCount: alerts.length,
                  itemBuilder: (_, i) => _AlertCard(
                    alert: alerts[i],
                    onResolve: () => service.resolveAlert(alerts[i].id),
                  ),
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}

class _AlertCard extends StatelessWidget {
  final AlertItem alert;
  final VoidCallback onResolve;
  const _AlertCard({required this.alert, required this.onResolve});

  @override
  Widget build(BuildContext context) {
    final isCritical = alert.type == 'critical';
    final color      = isCritical ? AppColors.danger : AppColors.warning;
    final fmt        = DateFormat('dd MMM, HH:mm');

    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Icon
            Container(
              width: 36, height: 36,
              decoration: BoxDecoration(
                color: color.withOpacity(0.1),
                shape: BoxShape.circle,
              ),
              child: Center(
                child: Text(alert.parameterIcon,
                  style: const TextStyle(fontSize: 16)),
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(children: [
                    Container(
                      padding: const EdgeInsets.symmetric(
                        horizontal: 6, vertical: 2),
                      decoration: BoxDecoration(
                        color: color.withOpacity(0.1),
                        borderRadius: BorderRadius.circular(4),
                      ),
                      child: Text(alert.type.toUpperCase(),
                        style: TextStyle(
                          fontSize: 10, fontWeight: FontWeight.bold,
                          color: color)),
                    ),
                    const SizedBox(width: 8),
                    Text(fmt.format(alert.timestamp),
                      style: TextStyle(fontSize: 11, color: Colors.grey[500])),
                    const Spacer(),
                    if (alert.resolved)
                      Container(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 6, vertical: 2),
                        decoration: BoxDecoration(
                          color: AppColors.safe.withOpacity(0.1),
                          borderRadius: BorderRadius.circular(4),
                        ),
                        child: Text('Resolved',
                          style: TextStyle(
                            fontSize: 10, color: AppColors.safe,
                            fontWeight: FontWeight.w600)),
                      ),
                  ]),
                  const SizedBox(height: 4),
                  Text(alert.message,
                    style: const TextStyle(fontSize: 13)),
                ],
              ),
            ),
            // Resolve button
            if (!alert.resolved)
              IconButton(
                icon: const Icon(Icons.check_circle_outline, size: 20),
                color: AppColors.safe,
                tooltip: 'Mark resolved',
                onPressed: onResolve,
              ),
          ],
        ),
      ),
    );
  }
}