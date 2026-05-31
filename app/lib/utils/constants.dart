// lib/utils/constants.dart
import 'package:flutter/material.dart';

class AppColors {
  static const primary     = Color(0xFF1A5276);
  static const safe        = Color(0xFF1E8449);
  static const warning     = Color(0xFFD4AC0D);
  static const danger      = Color(0xFF922B21);
  static const background  = Color(0xFFF4F6F7);
  static const cardBg      = Colors.white;
}

class AppThresholds {
  // Match exactly with config.h in firmware
  static const levelHigh      = 95.0;
  static const levelWarnLow   = 20.0;
  static const levelCritLow   = 10.0;
  static const phWarnLow      = 6.8;
  static const phWarnHigh     = 8.2;
  static const phCritLow      = 6.5;
  static const phCritHigh     = 8.5;
  static const turbidityWarn  = 1.0;
  static const turbidityCrit  = 5.0;
}

class AppStrings {
  static const appName    = 'SmAart Water System';
  static const deviceId   = 'node_01';
}