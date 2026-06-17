// lib/screens/settings_screen.dart
import 'package:flutter/material.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:provider/provider.dart';
import '../services/firestore_service.dart';
import '../utils/constants.dart';
import 'login_screen.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});
  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  bool _loading = true;
  bool _saving  = false;

  // Threshold controllers — pre-filled with defaults from constants.dart
  late final Map<String, TextEditingController> _ctrl;

  @override
  void initState() {
    super.initState();
    _ctrl = {
      'levelHigh':      TextEditingController(text: AppThresholds.levelHigh.toString()),
      'levelWarnLow':   TextEditingController(text: AppThresholds.levelWarnLow.toString()),
      'levelCritLow':   TextEditingController(text: AppThresholds.levelCritLow.toString()),
      'phCritLow':      TextEditingController(text: AppThresholds.phCritLow.toString()),
      'phCritHigh':     TextEditingController(text: AppThresholds.phCritHigh.toString()),
      'phWarnLow':      TextEditingController(text: AppThresholds.phWarnLow.toString()),
      'phWarnHigh':     TextEditingController(text: AppThresholds.phWarnHigh.toString()),
      'turbidityWarn':  TextEditingController(text: AppThresholds.turbidityWarn.toString()),
      'turbidityCrit':  TextEditingController(text: AppThresholds.turbidityCrit.toString()),
    };
    _loadFromFirestore();
  }

  @override
  void dispose() {
    for (final c in _ctrl.values) c.dispose();
    super.dispose();
  }

  Future<void> _loadFromFirestore() async {
    try {
      final svc = context.read<FirestoreService>();
      final saved = await svc.getThresholds();
      if (saved.isNotEmpty) {
        setState(() {
          for (final key in _ctrl.keys) {
            if (saved[key] != null) {
              _ctrl[key]!.text = saved[key].toString();
            }
          }
        });
      }
    } catch (_) {
      // Use defaults if fetch fails
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  Future<void> _save() async {
    setState(() => _saving = true);
    try {
      final thresholds = <String, double>{};
      for (final entry in _ctrl.entries) {
        final v = double.tryParse(entry.value.text);
        if (v != null) thresholds[entry.key] = v;
      }
      await context.read<FirestoreService>().saveThresholds(thresholds);
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Thresholds saved'),
            backgroundColor: AppColors.safe,
            duration: Duration(seconds: 2),
          ),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Save failed: $e'),
            backgroundColor: AppColors.danger),
        );
      }
    } finally {
      if (mounted) setState(() => _saving = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: const Text('Settings'),
        backgroundColor: AppColors.primary,
        foregroundColor: Colors.white,
        actions: [
          if (!_loading)
            TextButton(
              onPressed: _saving ? null : _save,
              child: _saving
                  ? const SizedBox(width: 18, height: 18,
                      child: CircularProgressIndicator(
                        color: Colors.white, strokeWidth: 2))
                  : const Text('Save',
                      style: TextStyle(
                        color: Colors.white, fontWeight: FontWeight.bold)),
            ),
        ],
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : ListView(
              padding: const EdgeInsets.all(16),
              children: [

                // ── Alert Thresholds ──────────────────────────
                _SectionHeader('Alert Thresholds'),
                const SizedBox(height: 4),
                Text(
                  'These values match BIS IS 10500:2012 drinking water standards. '
                  'Change only if your application requires different sensitivity.',
                  style: TextStyle(fontSize: 12, color: Colors.grey[600]),
                ),
                const SizedBox(height: 12),

                _ThresholdGroup(
                  title: 'Water Level (%)',
                  icon: Icons.water_drop,
                  fields: [
                    _ThresholdField('Auto-cutoff (close valve above)', _ctrl['levelHigh']!),
                    _ThresholdField('Low warning (below)', _ctrl['levelWarnLow']!),
                    _ThresholdField('Critical low (below)', _ctrl['levelCritLow']!),
                  ],
                ),
                const SizedBox(height: 12),

                _ThresholdGroup(
                  title: 'pH',
                  icon: Icons.science_outlined,
                  fields: [
                    _ThresholdField('Critical low (BIS min: 6.5)', _ctrl['phCritLow']!),
                    _ThresholdField('Critical high (BIS max: 8.5)', _ctrl['phCritHigh']!),
                    _ThresholdField('Warning low', _ctrl['phWarnLow']!),
                    _ThresholdField('Warning high', _ctrl['phWarnHigh']!),
                  ],
                ),
                const SizedBox(height: 12),

                _ThresholdGroup(
                  title: 'Turbidity (NTU)',
                  icon: Icons.water,
                  fields: [
                    _ThresholdField('Warning (above)', _ctrl['turbidityWarn']!),
                    _ThresholdField('Critical (BIS max: 5 NTU)', _ctrl['turbidityCrit']!),
                  ],
                ),
                const SizedBox(height: 28),

                // ── Account ───────────────────────────────────
                _SectionHeader('Account'),
                const SizedBox(height: 8),
                Card(
                  child: ListTile(
                    leading: const Icon(Icons.person_outline),
                    title: Text(
                      FirebaseAuth.instance.currentUser?.email ?? 'Unknown',
                      style: const TextStyle(fontSize: 14),
                    ),
                    subtitle: const Text('Logged in',
                      style: TextStyle(fontSize: 12)),
                  ),
                ),
                const SizedBox(height: 8),
                Card(
                  child: ListTile(
                    leading: const Icon(Icons.logout, color: AppColors.danger),
                    title: const Text('Sign Out',
                      style: TextStyle(color: AppColors.danger)),
                    onTap: () async {
                      await FirebaseAuth.instance.signOut();
                      if (context.mounted) {
                        Navigator.of(context).pushAndRemoveUntil(
                          MaterialPageRoute(
                            builder: (_) => const LoginScreen()),
                          (_) => false,
                        );
                      }
                    },
                  ),
                ),
                const SizedBox(height: 28),

                // ── About ─────────────────────────────────────
                _SectionHeader('About'),
                const SizedBox(height: 8),
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text(AppStrings.appName,
                          style: TextStyle(
                            fontWeight: FontWeight.bold, fontSize: 14)),
                        const SizedBox(height: 4),
                        Text('Version 1.0.0',
                          style: TextStyle(
                            fontSize: 12, color: Colors.grey[600])),
                        const SizedBox(height: 8),
                        Text(
                          'Supervisors: Prof. Amartansh Dubey & Prof. Jayadeva',
                          style: TextStyle(
                            fontSize: 12, color: Colors.grey[600])),
                        Text('Device ID: ${AppStrings.deviceId}',
                          style: TextStyle(
                            fontSize: 12, color: Colors.grey[600])),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 32),
              ],
            ),
    );
  }
}

class _SectionHeader extends StatelessWidget {
  final String title;
  const _SectionHeader(this.title);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 4),
      child: Text(title,
        style: const TextStyle(
          fontSize: 13,
          fontWeight: FontWeight.w700,
          color: AppColors.primary,
          letterSpacing: 0.5,
        )),
    );
  }
}

class _ThresholdGroup extends StatelessWidget {
  final String title;
  final IconData icon;
  final List<_ThresholdField> fields;
  const _ThresholdGroup({
    required this.title,
    required this.icon,
    required this.fields,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(children: [
              Icon(icon, size: 16, color: AppColors.primary),
              const SizedBox(width: 6),
              Text(title,
                style: const TextStyle(
                  fontWeight: FontWeight.w600, fontSize: 13)),
            ]),
            const SizedBox(height: 12),
            ...fields.map((f) => Padding(
              padding: const EdgeInsets.only(bottom: 10),
              child: f,
            )),
          ],
        ),
      ),
    );
  }
}

class _ThresholdField extends StatelessWidget {
  final String label;
  final TextEditingController controller;
  const _ThresholdField(this.label, this.controller);

  @override
  Widget build(BuildContext context) {
    return Row(children: [
      Expanded(
        flex: 3,
        child: Text(label,
          style: TextStyle(fontSize: 12, color: Colors.grey[700])),
      ),
      const SizedBox(width: 12),
      SizedBox(
        width: 80,
        child: TextFormField(
          controller: controller,
          keyboardType: const TextInputType.numberWithOptions(decimal: true),
          textAlign: TextAlign.center,
          style: const TextStyle(fontSize: 13),
          decoration: InputDecoration(
            isDense: true,
            contentPadding: const EdgeInsets.symmetric(
              horizontal: 8, vertical: 8),
            border: OutlineInputBorder(
              borderRadius: BorderRadius.circular(6)),
          ),
        ),
      ),
    ]);
  }
}