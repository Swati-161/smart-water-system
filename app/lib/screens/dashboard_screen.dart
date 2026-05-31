// lib/screens/dashboard_screen.dart

import 'package:flutter/material.dart';
import 'package:firebase_auth/firebase_auth.dart';

import '../services/firestore_service.dart';
import '../models/sensor_data.dart';
import 'login_screen.dart';

class DashboardScreen extends StatelessWidget {
  DashboardScreen({super.key});

  final FirestoreService _service = FirestoreService();

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('SmAart Water System'),
        actions: [
          IconButton(
            icon: const Icon(Icons.logout),
            tooltip: 'Sign Out',
            onPressed: () async {
              await FirebaseAuth.instance.signOut();

              Navigator.of(context).pushReplacement(
                MaterialPageRoute(
                  builder: (_) => const LoginScreen(),
                ),
              );
            },
          ),
        ],
      ),
      body: StreamBuilder<SensorData>(
        stream: _service.latestReadingStream(),
        builder: (context, snapshot) {
          if (!snapshot.hasData) {
            return const Center(
              child: CircularProgressIndicator(),
            );
          }

          final data = snapshot.data!;

          return SingleChildScrollView(
            padding: const EdgeInsets.all(16),
            child: Column(
              children: [
                _buildCard(
                  title: 'Water Level',
                  value: '${data.levelPct.toStringAsFixed(1)}%',
                  icon: Icons.water_drop,
                ),

                _buildCard(
                  title: 'pH Value',
                  value: data.ph.toStringAsFixed(2),
                  icon: Icons.science,
                ),

                _buildCard(
                  title: 'Turbidity',
                  value: '${data.turbidityNtu.toStringAsFixed(2)} NTU',
                  icon: Icons.opacity,
                ),

                _buildCard(
                  title: 'Flow Rate',
                  value: '${data.flowLpm.toStringAsFixed(1)} L/min',
                  icon: Icons.speed,
                ),

                _buildCard(
                  title: 'Leak Status',
                  value:
                      data.leakDetected ? 'LEAK DETECTED' : 'No Leak Detected',
                  icon: Icons.warning,
                  color:
                      data.leakDetected ? Colors.red : Colors.green,
                ),
              ],
            ),
          );
        },
      ),
    );
  }

  Widget _buildCard({
    required String title,
    required String value,
    required IconData icon,
    Color? color,
  }) {
    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      child: ListTile(
        leading: Icon(icon, color: color),
        title: Text(title),
        subtitle: Text(
          value,
          style: const TextStyle(
            fontSize: 18,
            fontWeight: FontWeight.bold,
          ),
        ),
      ),
    );
  }
}