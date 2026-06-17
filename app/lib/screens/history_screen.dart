// lib/screens/history_screen.dart
import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import 'package:provider/provider.dart';
import '../services/firestore_service.dart';
import '../models/sensor_data.dart';
import '../utils/constants.dart';

class HistoryScreen extends StatefulWidget {
  const HistoryScreen({super.key});
  @override
  State<HistoryScreen> createState() => _HistoryScreenState();
}

class _HistoryScreenState extends State<HistoryScreen>
    with SingleTickerProviderStateMixin {
  late TabController _tabs;
  int _days = 7;
  bool _loading = false;
  List<SensorData> _history = [];
  List<Map<String, dynamic>> _daily = [];
  String? _error;

  @override
  void initState() {
    super.initState();
    _tabs = TabController(length: 4, vsync: this);
    _load();
  }

  @override
  void dispose() {
    _tabs.dispose();
    super.dispose();
  }

  Future<void> _load() async {
    setState(() { _loading = true; _error = null; });
    try {
      final svc = context.read<FirestoreService>();
      final results = await Future.wait([
        svc.getHistory(_days),
        svc.getDailyConsumption(),
      ]);
      setState(() {
        _history = results[0] as List<SensorData>;
        _daily   = results[1] as List<Map<String, dynamic>>;
      });
    } catch (e) {
      setState(() => _error = e.toString());
    } finally {
      setState(() => _loading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: const Text('History'),
        backgroundColor: AppColors.primary,
        foregroundColor: Colors.white,
        actions: [
          // Days selector
          PopupMenuButton<int>(
            initialValue: _days,
            onSelected: (v) { setState(() => _days = v); _load(); },
            itemBuilder: (_) => [7, 14, 30].map((d) =>
              PopupMenuItem(value: d, child: Text('$d days'))).toList(),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 12),
              child: Row(children: [
                Text('$_days days',
                  style: const TextStyle(color: Colors.white, fontSize: 14)),
                const Icon(Icons.arrow_drop_down, color: Colors.white),
              ]),
            ),
          ),
        ],
        bottom: TabBar(
          controller: _tabs,
          labelColor: Colors.white,
          unselectedLabelColor: Colors.white60,
          indicatorColor: Colors.white,
          tabs: const [
            Tab(text: 'Level'),
            Tab(text: 'pH'),
            Tab(text: 'Turbidity'),
            Tab(text: 'Usage'),
          ],
        ),
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : _error != null
              ? Center(child: Text('Error: $_error'))
              : _history.isEmpty
                  ? const Center(child: Text('No data for selected period'))
                  : TabBarView(
                      controller: _tabs,
                      children: [
                        _LineChartTab(
                          history: _history,
                          getValue: (d) => d.levelPct,
                          label: 'Water Level (%)',
                          color: AppColors.primary,
                          minY: 0, maxY: 100,
                          thresholdLines: [
                            _ThresholdLine(AppThresholds.levelHigh,    AppColors.danger,  'Full'),
                            _ThresholdLine(AppThresholds.levelWarnLow, AppColors.warning, 'Low'),
                          ],
                        ),
                        _LineChartTab(
                          history: _history,
                          getValue: (d) => d.ph,
                          label: 'pH',
                          color: AppColors.safe,
                          minY: 0, maxY: 14,
                          thresholdLines: [
                            _ThresholdLine(AppThresholds.phCritHigh, AppColors.danger,  'Max BIS'),
                            _ThresholdLine(AppThresholds.phCritLow,  AppColors.danger,  'Min BIS'),
                          ],
                        ),
                        _LineChartTab(
                          history: _history,
                          getValue: (d) => d.turbidityNtu,
                          label: 'Turbidity (NTU)',
                          color: AppColors.warning,
                          minY: 0, maxY: 20,
                          thresholdLines: [
                            _ThresholdLine(AppThresholds.turbidityCrit, AppColors.danger,  'BIS limit'),
                            _ThresholdLine(AppThresholds.turbidityWarn, AppColors.warning, 'Warning'),
                          ],
                        ),
                        _BarChartTab(daily: _daily),
                      ],
                    ),
    );
  }
}

class _ThresholdLine {
  final double value;
  final Color color;
  final String label;
  const _ThresholdLine(this.value, this.color, this.label);
}

class _LineChartTab extends StatelessWidget {
  final List<SensorData> history;
  final double Function(SensorData) getValue;
  final String label;
  final Color color;
  final double minY, maxY;
  final List<_ThresholdLine> thresholdLines;

  const _LineChartTab({
    required this.history,
    required this.getValue,
    required this.label,
    required this.color,
    required this.minY,
    required this.maxY,
    required this.thresholdLines,
  });

  @override
  Widget build(BuildContext context) {
    if (history.isEmpty) {
      return const Center(child: Text('No data'));
    }

    final spots = history.asMap().entries.map((e) =>
      FlSpot(e.key.toDouble(), getValue(e.value).clamp(minY, maxY))).toList();

    final values = spots.map((s) => s.y).toList();
    final minVal = values.reduce((a, b) => a < b ? a : b);
    final maxVal = values.reduce((a, b) => a > b ? a : b);
    final avg    = values.reduce((a, b) => a + b) / values.length;

    return Padding(
      padding: const EdgeInsets.all(16),
      child: Column(
        children: [
          // Summary cards
          Row(children: [
            _StatChip('Min',  minVal.toStringAsFixed(1), AppColors.safe),
            const SizedBox(width: 8),
            _StatChip('Avg',  avg.toStringAsFixed(1),    AppColors.primary),
            const SizedBox(width: 8),
            _StatChip('Max',  maxVal.toStringAsFixed(1), AppColors.danger),
          ]),
          const SizedBox(height: 16),
          Expanded(
            child: LineChart(
              LineChartData(
                minY: minY,
                maxY: maxY,
                gridData: FlGridData(
                  show: true,
                  drawVerticalLine: false,
                  getDrawingHorizontalLine: (_) => FlLine(
                    color: Colors.grey[300]!, strokeWidth: 0.5),
                ),
                borderData: FlBorderData(show: false),
                titlesData: FlTitlesData(
                  leftTitles: AxisTitles(
                    sideTitles: SideTitles(
                      showTitles: true, reservedSize: 40,
                      getTitlesWidget: (v, _) => Text(
                        v.toStringAsFixed(0),
                        style: const TextStyle(fontSize: 10)),
                    ),
                  ),
                  bottomTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                  rightTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                  topTitles:   AxisTitles(sideTitles: SideTitles(showTitles: false)),
                ),
                // Threshold reference lines
                extraLinesData: ExtraLinesData(
                  horizontalLines: thresholdLines.map((t) =>
                    HorizontalLine(
                      y: t.value,
                      color: t.color.withOpacity(0.7),
                      strokeWidth: 1,
                      dashArray: [5, 4],
                      label: HorizontalLineLabel(
                        show: true,
                        labelResolver: (_) => t.label,
                        style: TextStyle(
                          fontSize: 10, color: t.color,
                          fontWeight: FontWeight.w600),
                      ),
                    )).toList(),
                ),
                lineBarsData: [
                  LineChartBarData(
                    spots: spots,
                    isCurved: true,
                    color: color,
                    barWidth: 2,
                    dotData: FlDotData(show: false),
                    belowBarData: BarAreaData(
                      show: true,
                      color: color.withOpacity(0.08),
                    ),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 8),
          Text(label,
            style: TextStyle(fontSize: 12, color: Colors.grey[600])),
        ],
      ),
    );
  }
}

class _BarChartTab extends StatelessWidget {
  final List<Map<String, dynamic>> daily;
  const _BarChartTab({required this.daily});

  @override
  Widget build(BuildContext context) {
    if (daily.isEmpty) {
      return const Center(
        child: Text('Daily usage data appears here after midnight each day'));
    }

    // Reverse so oldest is on the left
    final items = daily.reversed.toList();

    return Padding(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text('Daily Water Consumption (litres)',
            style: TextStyle(
              fontSize: 13, fontWeight: FontWeight.w600,
              color: Colors.grey[700])),
          const SizedBox(height: 16),
          Expanded(
            child: BarChart(
              BarChartData(
                alignment: BarChartAlignment.spaceAround,
                barGroups: items.asMap().entries.map((e) =>
                  BarChartGroupData(x: e.key, barRods: [
                    BarChartRodData(
                      toY: (e.value['litres'] as num? ?? 0).toDouble(),
                      color: AppColors.primary,
                      width: 20,
                      borderRadius: const BorderRadius.vertical(
                        top: Radius.circular(4)),
                    ),
                  ])).toList(),
                titlesData: FlTitlesData(
                  bottomTitles: AxisTitles(
                    sideTitles: SideTitles(
                      showTitles: true, reservedSize: 28,
                      getTitlesWidget: (v, _) {
                        final idx = v.toInt();
                        if (idx < 0 || idx >= items.length) {
                          return const SizedBox();
                        }
                        final date = items[idx]['date'] as String? ?? '';
                        return Text(date,
                          style: const TextStyle(fontSize: 9));
                      },
                    ),
                  ),
                  leftTitles: AxisTitles(
                    sideTitles: SideTitles(
                      showTitles: true, reservedSize: 40,
                      getTitlesWidget: (v, _) => Text(
                        '${v.toInt()}L',
                        style: const TextStyle(fontSize: 10)),
                    ),
                  ),
                  rightTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                  topTitles:   AxisTitles(sideTitles: SideTitles(showTitles: false)),
                ),
                gridData: FlGridData(
                  show: true, drawVerticalLine: false,
                  getDrawingHorizontalLine: (_) => FlLine(
                    color: Colors.grey[300]!, strokeWidth: 0.5),
                ),
                borderData: FlBorderData(show: false),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _StatChip extends StatelessWidget {
  final String label, value;
  final Color color;
  const _StatChip(this.label, this.value, this.color);

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Container(
        padding: const EdgeInsets.symmetric(vertical: 8),
        decoration: BoxDecoration(
          color: color.withOpacity(0.08),
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: color.withOpacity(0.2)),
        ),
        child: Column(children: [
          Text(label,
            style: TextStyle(fontSize: 11, color: Colors.grey[600])),
          Text(value,
            style: TextStyle(
              fontSize: 16, fontWeight: FontWeight.bold, color: color)),
        ]),
      ),
    );
  }
}