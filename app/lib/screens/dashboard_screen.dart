import 'package:flutter/material.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:provider/provider.dart';
import '../services/firestore_service.dart';
import '../models/sensor_data.dart';
import '../widgets/level_gauge.dart';
import '../widgets/ph_card.dart';
import '../widgets/sensor_card.dart';
import '../widgets/status_badge.dart';
import '../utils/constants.dart';
import 'login_screen.dart';
import 'history_screen.dart';
import 'alerts_screen.dart';
import 'valve_screen.dart';
import 'settings_screen.dart';

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({super.key});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  int _currentIndex = 0;

  final List<Widget> _screens = const [
    _DashboardBody(),
    HistoryScreen(),
    AlertsScreen(),
    ValveScreen(),
    SettingsScreen(),
  ];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: _screens[_currentIndex],
      bottomNavigationBar: NavigationBar(
        selectedIndex: _currentIndex,
        onDestinationSelected: (i) => setState(() => _currentIndex = i),
        destinations: const [
          NavigationDestination(
            icon: Icon(Icons.dashboard_outlined),
            selectedIcon: Icon(Icons.dashboard),
            label: 'Dashboard',
          ),
          NavigationDestination(
            icon: Icon(Icons.show_chart_outlined),
            selectedIcon: Icon(Icons.show_chart),
            label: 'History',
          ),
          NavigationDestination(
            icon: Icon(Icons.notifications_outlined),
            selectedIcon: Icon(Icons.notifications),
            label: 'Alerts',
          ),
          NavigationDestination(
            icon: Icon(Icons.toggle_off_outlined),
            selectedIcon: Icon(Icons.toggle_on),
            label: 'Valve',
          ),
          NavigationDestination(
            icon: Icon(Icons.settings_outlined),
            selectedIcon: Icon(Icons.settings),
            label: 'Settings',
          ),
        ],
      ),
    );
  }
}

class _DashboardBody extends StatelessWidget {
  const _DashboardBody();

  @override
  Widget build(BuildContext context) {
    final service = context.read<FirestoreService>();

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: const Text(AppStrings.appName),
        backgroundColor: AppColors.primary,
        foregroundColor: Colors.white,
        actions: [
          // Device status indicator
          StreamBuilder<String>(
            stream: service.deviceStatusStream(),
            builder: (_, snap) => Padding(
              padding: const EdgeInsets.only(right: 12),
              child: StatusBadge(status: snap.data ?? 'unknown'),
            ),
          ),
          // Logout
          IconButton(
            icon: const Icon(Icons.logout),
            onPressed: () async {
              await FirebaseAuth.instance.signOut();
              if (context.mounted) {
                Navigator.of(context).pushReplacement(
                  MaterialPageRoute(builder: (_) => const LoginScreen()),
                );
              }
            },
          ),
        ],
      ),
      body: StreamBuilder<SensorData>(
        stream: service.latestReadingStream(),
        builder: (context, snapshot) {
          if (snapshot.hasError) {
            return Center(child: Text('Error: ${snapshot.error}'));
          }
          if (!snapshot.hasData) {
            return const Center(child: CircularProgressIndicator());
          }

          final data = snapshot.data!;

          return RefreshIndicator(
            onRefresh: () async {}, // stream auto-refreshes
            child: SingleChildScrollView(
              physics: const AlwaysScrollableScrollPhysics(),
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [

                  // Mock data banner
                  if (data.mock)
                    Container(
                      margin: const EdgeInsets.only(bottom: 12),
                      padding: const EdgeInsets.all(10),
                      decoration: BoxDecoration(
                        color: Colors.amber[50],
                        borderRadius: BorderRadius.circular(8),
                        border: Border.all(color: Colors.amber[300]!),
                      ),
                      child: Row(children: [
                        Icon(Icons.info_outline,
                          color: Colors.amber[700], size: 16),
                        const SizedBox(width: 8),
                        Text('Simulated data — hardware not connected',
                          style: TextStyle(
                            fontSize: 12,
                            color: Colors.amber[800],
                          )),
                      ]),
                    ),

                  // Water level gauge
                  LevelGauge(percentage: data.levelPct),
                  const SizedBox(height: 12),

                  // pH and turbidity row
                  Row(children: [
                    Expanded(child: PHCard(pH: data.ph)),
                    const SizedBox(width: 12),
                    Expanded(
                      child: SensorCard(
                        label: 'Turbidity',
                        value: data.turbidityNtu >= 0
                            ? '${data.turbidityNtu.toStringAsFixed(1)} NTU'
                            : '--',
                        icon:  Icons.water,
                        color: data.turbidityNtu > AppThresholds.turbidityCrit
                            ? AppColors.danger
                            : data.turbidityNtu > AppThresholds.turbidityWarn
                                ? AppColors.warning
                                : AppColors.safe,
                        subtitle: data.turbidityNtu <= AppThresholds.turbidityWarn
                            ? 'Clean' : 'Cloudy',
                      ),
                    ),
                  ]),
                  const SizedBox(height: 12),

                  // Flow and leak row
                  Row(children: [
                    Expanded(
                      child: SensorCard(
                        label:    'Flow Rate',
                        value:    '${data.flowLpm.toStringAsFixed(1)} L/min',
                        icon:     Icons.speed,
                        subtitle: 'Today: ${data.totalLitres.toStringAsFixed(0)}L used',
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: SensorCard(
                        label: 'Leak Status',
                        value: data.leakDetected ? 'LEAK!' : 'None',
                        icon:  data.leakDetected
                            ? Icons.warning_amber
                            : Icons.check_circle_outline,
                        color: data.leakDetected
                            ? AppColors.danger
                            : AppColors.safe,
                      ),
                    ),
                  ]),
                  const SizedBox(height: 12),

                  // Battery card
                  SensorCard(
                    label:    'Battery',
                    value:    '${data.batteryV.toStringAsFixed(2)} V',
                    icon:     Icons.battery_charging_full,
                    subtitle: data.batteryV > 3.5
                        ? 'Good' : 'Low — check solar panel',
                    color:    data.batteryV > 3.5
                        ? AppColors.safe : AppColors.warning,
                  ),

                  const SizedBox(height: 8),
                  // Last updated
                  Center(
                    child: Text(
                      'Last updated: ${_formatTime(data.timestamp)}',
                      style: TextStyle(
                        fontSize: 11,
                        color: Colors.grey[500],
                      ),
                    ),
                  ),
                ],
              ),
            ),
          );
        },
      ),
    );
  }

  String _formatTime(DateTime t) {
    return '${t.hour.toString().padLeft(2,'0')}:'
           '${t.minute.toString().padLeft(2,'0')}:'
           '${t.second.toString().padLeft(2,'0')}';
  }
}