// ============================================================
//  Smart Parking System — Flutter Web
//  lib/main.dart  •  Models + API Service + App Entry
// ============================================================

import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:http/http.dart' as http;
import 'dashboard.dart';

// ── Color Palette ─────────────────────────────────────────────

class AppColors {
  static const bg       = Color(0xFF0D1117);
  static const surface  = Color(0xFF161B22);
  static const surface2 = Color(0xFF1C2230);
  static const border   = Color(0xFF30363D);
  static const amber    = Color(0xFFF0A500);
  static const amberDim = Color(0xFF7A5200);
  static const green    = Color(0xFF3FB950);
  static const greenDim = Color(0xFF1A3D22);
  static const red      = Color(0xFFF85149);
  static const redDim   = Color(0xFF3D1A1A);
  static const text     = Color(0xFFE6EDF3);
  static const muted    = Color(0xFF8B949E);
  static const mutedDim = Color(0xFF3D444D);
}

// ── Models ────────────────────────────────────────────────────

class SlotInfo {
  final String status;
  final String? uid;
  final String? bookingId;
  final String? name;
  final String? bookedAt;

  const SlotInfo({
    required this.status,
    this.uid,
    this.bookingId,
    this.name,
    this.bookedAt,
  });

  bool get isOccupied => status == 'occupied';

  factory SlotInfo.fromJson(Map<String, dynamic> j) => SlotInfo(
        status: j['status'] ?? 'available',
        uid: j['uid'],
        bookingId: j['bookingId'],
        name: j['name'],
        bookedAt: j['bookedAt'],
      );
}

class ParkingStats {
  final int permanentFree;
  final int permanentOccupied;
  final int visitorFree;
  final int visitorOccupied;
  final int totalFree;

  const ParkingStats({
    required this.permanentFree,
    required this.permanentOccupied,
    required this.visitorFree,
    required this.visitorOccupied,
    required this.totalFree,
  });

  factory ParkingStats.fromJson(Map<String, dynamic> j) => ParkingStats(
        permanentFree: j['permanentFree'] ?? 4,
        permanentOccupied: j['permanentOccupied'] ?? 0,
        visitorFree: j['visitorFree'] ?? 4,
        visitorOccupied: j['visitorOccupied'] ?? 0,
        totalFree: j['totalFree'] ?? 8,
      );
}

class ParkingData {
  final Map<int, SlotInfo> permanent;
  final Map<int, SlotInfo> visitor;
  final ParkingStats stats;

  const ParkingData({
    required this.permanent,
    required this.visitor,
    required this.stats,
  });

  factory ParkingData.fromJson(Map<String, dynamic> json) {
    final slots    = json['slots'] as Map<String, dynamic>;
    final permJson = slots['permanent'] as Map<String, dynamic>;
    final visJson  = slots['visitor']   as Map<String, dynamic>;

    return ParkingData(
      permanent: permJson.map((k, v) => MapEntry(int.parse(k), SlotInfo.fromJson(v))),
      visitor:   visJson.map((k, v)  => MapEntry(int.parse(k), SlotInfo.fromJson(v))),
      stats:     ParkingStats.fromJson(json['stats']),
    );
  }
}

// ── API Service ───────────────────────────────────────────────

class ApiService {
  // ← Change to your server's IP address
  static const _base = 'http://localhost:3000';

  static Future<ParkingData?> fetchSlots() async {
    try {
      final res = await http
          .get(Uri.parse('$_base/slots'))
          .timeout(const Duration(seconds: 8));
      if (res.statusCode == 200) {
        return ParkingData.fromJson(jsonDecode(res.body));
      }
    } catch (e) {
      debugPrint('fetchSlots error: $e');
    }
    return null;
  }

  static Future<Map<String, dynamic>?> bookSlot(String name) async {
    try {
      final res = await http.post(
        Uri.parse('$_base/book'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({'name': name.isEmpty ? 'Guest' : name}),
      );
      return jsonDecode(res.body);
    } catch (e) {
      debugPrint('bookSlot error: $e');
    }
    return null;
  }

  static Future<Map<String, dynamic>?> releaseSlot(String bookingId) async {
    try {
      final res = await http.post(
        Uri.parse('$_base/release'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({'bookingId': bookingId}),
      );
      return jsonDecode(res.body);
    } catch (e) {
      debugPrint('releaseSlot error: $e');
    }
    return null;
  }
}

// ── App Entry ─────────────────────────────────────────────────

void main() {
  runApp(const SmartParkApp());
}

class SmartParkApp extends StatelessWidget {
  const SmartParkApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'SmartPark',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        scaffoldBackgroundColor: AppColors.bg,
        colorScheme: const ColorScheme.dark(
          primary: AppColors.amber,
          surface: AppColors.surface,
        ),
        textTheme: GoogleFonts.rajdhaniTextTheme(ThemeData.dark().textTheme)
            .apply(bodyColor: AppColors.text, displayColor: AppColors.text),
        inputDecorationTheme: InputDecorationTheme(
          filled: true,
          fillColor: AppColors.bg,
          border: OutlineInputBorder(
            borderRadius: BorderRadius.circular(10),
            borderSide: const BorderSide(color: AppColors.border),
          ),
          enabledBorder: OutlineInputBorder(
            borderRadius: BorderRadius.circular(10),
            borderSide: const BorderSide(color: AppColors.border),
          ),
          focusedBorder: OutlineInputBorder(
            borderRadius: BorderRadius.circular(10),
            borderSide: const BorderSide(color: AppColors.amber, width: 1.5),
          ),
          hintStyle: const TextStyle(color: AppColors.muted, fontSize: 15),
          contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
        ),
        elevatedButtonTheme: ElevatedButtonThemeData(
          style: ElevatedButton.styleFrom(
            backgroundColor: AppColors.amber,
            foregroundColor: Colors.black,
            elevation: 0,
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
            textStyle: GoogleFonts.rajdhani(
              fontWeight: FontWeight.w700,
              fontSize: 15,
              letterSpacing: .5,
            ),
            padding: const EdgeInsets.symmetric(horizontal: 28, vertical: 14),
          ),
        ),
        snackBarTheme: SnackBarThemeData(
          behavior: SnackBarBehavior.floating,
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
        ),
      ),
      home: const DashboardScreen(),
    );
  }
}
