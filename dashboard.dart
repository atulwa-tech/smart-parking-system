// ============================================================
//  Smart Parking System — Flutter Web
//  lib/dashboard.dart  •  Full Dashboard UI
// ============================================================

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'main.dart';

// ─────────────────────────────────────────────────────────────
//  Dashboard Screen
// ─────────────────────────────────────────────────────────────

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({super.key});
  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen>
    with TickerProviderStateMixin {
  ParkingData? _data;
  bool _initialLoad = true;
  bool _refreshing  = false;
  bool _booking     = false;
  Timer? _timer;
  final _nameCtrl = TextEditingController();

  late AnimationController _pulseCtrl;
  late Animation<double> _pulseAnim;

  @override
  void initState() {
    super.initState();
    _pulseCtrl = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1400),
    )..repeat(reverse: true);
    _pulseAnim = Tween<double>(begin: 0.3, end: 1.0).animate(
      CurvedAnimation(parent: _pulseCtrl, curve: Curves.easeInOut),
    );

    _fetch();
    _timer = Timer.periodic(const Duration(seconds: 15), (_) => _fetch());
  }

  @override
  void dispose() {
    _timer?.cancel();
    _pulseCtrl.dispose();
    _nameCtrl.dispose();
    super.dispose();
  }

  Future<void> _fetch({bool manual = false}) async {
    if (manual) setState(() => _refreshing = true);
    final data = await ApiService.fetchSlots();
    if (!mounted) return;
    setState(() {
      _data        = data;
      _initialLoad = false;
      _refreshing  = false;
    });
    if (manual && data == null) {
      _toast('⚠ Cannot reach server. Is the backend running?', false);
    }
  }

  Future<void> _book() async {
    setState(() => _booking = true);
    final res = await ApiService.bookSlot(_nameCtrl.text.trim());
    if (!mounted) return;
    setState(() => _booking = false);
    if (res != null && res['success'] == true) {
      _nameCtrl.clear();
      _toast('✅ Slot ${res['slot']} booked!  ID: ${res['bookingId']}', true);
      _fetch();
    } else {
      _toast('❌ ${res?['message'] ?? 'Booking failed'}', false);
    }
  }

  Future<void> _release(String bookingId) async {
    final res = await ApiService.releaseSlot(bookingId);
    if (!mounted) return;
    if (res != null && res['success'] == true) {
      _toast('✅ Slot ${res['slot']} released', true);
      _fetch();
    } else {
      _toast('❌ ${res?['message'] ?? 'Release failed'}', false);
    }
  }

  Future<void> _releasePermanent(String uid) async {
    final res = await ApiService.releasePermanentSlot(uid);
    if (!mounted) return;
    if (res != null && res['success'] == true) {
      _toast('✅ Permanent Slot ${res['slot']} released', true);
      _fetch();
    } else {
      _toast('❌ ${res?['message'] ?? 'Release failed'}', false);
    }
  }

  void _toast(String msg, bool ok) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(msg,
            style: GoogleFonts.rajdhani(
                fontWeight: FontWeight.w600, color: Colors.white)),
        backgroundColor: ok ? const Color(0xFF1F4D2A) : const Color(0xFF4D1F1F),
        duration: const Duration(seconds: 3),
        margin: const EdgeInsets.all(16),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(10),
          side: BorderSide(
            color: ok ? AppColors.green : AppColors.red,
            width: .8,
          ),
        ),
      ),
    );
  }

  // ── Build ───────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    if (_initialLoad) {
      return Scaffold(
        backgroundColor: AppColors.bg,
        body: Center(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const CircularProgressIndicator(
                  color: AppColors.amber, strokeWidth: 2),
              const SizedBox(height: 20),
              Text('Connecting to SmartPark...',
                  style: GoogleFonts.rajdhani(
                      color: AppColors.muted, fontSize: 16)),
            ],
          ),
        ),
      );
    }

    final stats = _data?.stats;
    final noData = _data == null;

    return Scaffold(
      backgroundColor: AppColors.bg,
      body: SingleChildScrollView(
        child: Center(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 1120),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 24),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  // ── Header
                  _Header(
                    refreshing: _refreshing,
                    onRefresh: () => _fetch(manual: true),
                    pulse: _pulseAnim,
                  ),

                  if (noData) ...[
                    const SizedBox(height: 60),
                    _OfflineBanner(onRetry: () => _fetch(manual: true)),
                    const SizedBox(height: 60),
                  ] else ...[
                    const SizedBox(height: 28),

                    // ── Stat Cards
                    _StatsRow(stats: stats!),
                    const SizedBox(height: 20),

                    // ── Progress Bars
                    _ProgressRow(stats: stats),
                    const SizedBox(height: 36),

                    // ── Permanent Slots
                    _SectionLabel(icon: '🔑', title: 'Permanent Slots', sub: 'RFID · Slots 1–4'),
                    const SizedBox(height: 14),
                    _SlotGrid(
                      slots: _data!.permanent,
                      isPermanent: true,
                      onRelease: _releasePermanent,
                    ),
                    const SizedBox(height: 36),

                    // ── Visitor Slots
                    _SectionLabel(icon: '🌐', title: 'Visitor Slots', sub: 'Web Booking · Slots 5–8'),
                    const SizedBox(height: 14),
                    _SlotGrid(
                      slots: _data!.visitor,
                      isPermanent: false,
                      onRelease: _release,
                    ),
                    const SizedBox(height: 36),

                    // ── Booking Panel
                    _SectionLabel(icon: '📋', title: 'Book a Visitor Slot', sub: ''),
                    const SizedBox(height: 14),
                    _BookingPanel(
                      controller: _nameCtrl,
                      loading: _booking,
                      disabled: stats.visitorFree == 0,
                      onBook: _book,
                    ),
                    const SizedBox(height: 36),

                    // ── Active Bookings
                    _SectionLabel(icon: '🧾', title: 'Active Visitor Bookings', sub: ''),
                    const SizedBox(height: 14),
                    _ActiveBookings(
                      visitor: _data!.visitor,
                      onRelease: _release,
                    ),
                    const SizedBox(height: 60),
                  ],
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

// ─────────────────────────────────────────────────────────────
//  Header
// ─────────────────────────────────────────────────────────────

class _Header extends StatelessWidget {
  final bool refreshing;
  final VoidCallback onRefresh;
  final Animation<double> pulse;

  const _Header({
    required this.refreshing,
    required this.onRefresh,
    required this.pulse,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(vertical: 24),
      decoration: const BoxDecoration(
        border: Border(bottom: BorderSide(color: AppColors.border, width: .8)),
      ),
      child: Row(
        children: [
          // Brand icon
          Container(
            width: 46,
            height: 46,
            decoration: BoxDecoration(
              gradient: const LinearGradient(
                colors: [AppColors.amber, Color(0xFFA06A00)],
                begin: Alignment.topLeft,
                end: Alignment.bottomRight,
              ),
              borderRadius: BorderRadius.circular(12),
            ),
            child: const Center(
              child: Text('🅿', style: TextStyle(fontSize: 22)),
            ),
          ),
          const SizedBox(width: 14),
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('SmartPark',
                  style: GoogleFonts.rajdhani(
                      fontSize: 24, fontWeight: FontWeight.w800,
                      color: AppColors.text, letterSpacing: -.3)),
              Text('PARKING MANAGEMENT SYSTEM',
                  style: GoogleFonts.spaceMono(
                      fontSize: 10, color: AppColors.muted,
                      letterSpacing: 1.5)),
            ],
          ),
          const Spacer(),
          // Live badge
          AnimatedBuilder(
            animation: pulse,
            builder: (_, __) => Container(
              padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 7),
              decoration: BoxDecoration(
                color: AppColors.green.withValues(alpha: 0.07),
                border: Border.all(color: AppColors.green.withValues(alpha: 0.3)),
                borderRadius: BorderRadius.circular(20),
              ),
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Container(
                    width: 7, height: 7,
                    decoration: BoxDecoration(
                      shape: BoxShape.circle,
                      color: AppColors.green.withValues(alpha: pulse.value),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Text('LIVE',
                      style: GoogleFonts.spaceMono(
                          fontSize: 11, color: AppColors.green,
                          fontWeight: FontWeight.w700)),
                ],
              ),
            ),
          ),
          const SizedBox(width: 12),
          // Refresh button
          TextButton.icon(
            onPressed: refreshing ? null : onRefresh,
            style: TextButton.styleFrom(
              foregroundColor: refreshing ? AppColors.muted : AppColors.muted,
              backgroundColor: Colors.transparent,
              shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(8),
                  side: const BorderSide(color: AppColors.border)),
              padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
            ),
            icon: refreshing
                ? const SizedBox(
                    width: 14, height: 14,
                    child: CircularProgressIndicator(strokeWidth: 1.5, color: AppColors.amber))
                : const Icon(Icons.refresh_rounded, size: 15),
            label: Text('Refresh',
                style: GoogleFonts.spaceMono(fontSize: 11, letterSpacing: .5)),
          ),
        ],
      ),
    );
  }
}

// ─────────────────────────────────────────────────────────────
//  Stats Row
// ─────────────────────────────────────────────────────────────

class _StatsRow extends StatelessWidget {
  final ParkingStats stats;
  const _StatsRow({required this.stats});

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(builder: (_, c) {
      final wide = c.maxWidth > 600;
      final cards = [
        _StatCard(label: 'Total Available', value: '${stats.totalFree}',
            sub: 'out of 8 slots', color: AppColors.green),
        _StatCard(label: 'Permanent Free', value: '${stats.permanentFree}',
            sub: 'RFID slots 1–4', color: AppColors.amber),
        _StatCard(label: 'Visitor Free', value: '${stats.visitorFree}',
            sub: 'Web slots 5–8', color: const Color(0xFF58A6FF)),
      ];
      return wide
          ? Row(children: cards
              .map((c) => Expanded(child: c))
              .expand((w) => [w, const SizedBox(width: 14)])
              .toList()
            ..removeLast())
          : Wrap(spacing: 14, runSpacing: 14, children: cards.map((c) =>
              SizedBox(width: c.maxWidth(context), child: c)).toList());
    });
  }
}

class _StatCard extends StatelessWidget {
  final String label;
  final String value;
  final String sub;
  final Color color;
  const _StatCard({required this.label, required this.value, required this.sub, required this.color});

  double maxWidth(BuildContext context) {
    final w = MediaQuery.of(context).size.width - 48;
    return w > 600 ? w / 3 - 10 : (w / 2 - 7).clamp(120, 300);
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(22),
      decoration: BoxDecoration(
        color: AppColors.surface,
        border: Border.all(color: AppColors.border, width: .8),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(label.toUpperCase(),
              style: GoogleFonts.spaceMono(
                  fontSize: 10, color: AppColors.muted, letterSpacing: 1.3)),
          const SizedBox(height: 10),
          Text(value,
              style: GoogleFonts.spaceMono(
                  fontSize: 52, fontWeight: FontWeight.w700,
                  color: color, height: 1)),
          const SizedBox(height: 6),
          Text(sub,
              style: GoogleFonts.rajdhani(fontSize: 13, color: AppColors.muted)),
        ],
      ),
    );
  }
}

// ─────────────────────────────────────────────────────────────
//  Progress Row
// ─────────────────────────────────────────────────────────────

class _ProgressRow extends StatelessWidget {
  final ParkingStats stats;
  const _ProgressRow({required this.stats});

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(builder: (_, c) {
      final wide = c.maxWidth > 600;
      final blocks = [
        _ProgressBlock(
          title: '🔑 Permanent Occupancy',
          occupied: stats.permanentOccupied,
          total: 4,
          color: AppColors.amber,
        ),
        _ProgressBlock(
          title: '🌐 Visitor Occupancy',
          occupied: stats.visitorOccupied,
          total: 4,
          color: const Color(0xFF58A6FF),
        ),
      ];
      return wide
          ? Row(
              children: blocks
                  .map((b) => Expanded(child: b))
                  .expand((w) => [w, const SizedBox(width: 14)])
                  .toList()
                ..removeLast())
          : Column(children: [blocks[0], const SizedBox(height: 14), blocks[1]]);
    });
  }
}

class _ProgressBlock extends StatelessWidget {
  final String title;
  final int occupied;
  final int total;
  final Color color;

  const _ProgressBlock({
    required this.title,
    required this.occupied,
    required this.total,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    final pct = total == 0 ? 0.0 : occupied / total;
    return Container(
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: AppColors.surface,
        border: Border.all(color: AppColors.border, width: .8),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(title,
                  style: GoogleFonts.rajdhani(
                      fontSize: 14, color: AppColors.text, fontWeight: FontWeight.w600)),
              Text('${(pct * 100).round()}%',
                  style: GoogleFonts.spaceMono(
                      fontSize: 12, color: color, fontWeight: FontWeight.w700)),
            ],
          ),
          const SizedBox(height: 12),
          ClipRRect(
            borderRadius: BorderRadius.circular(4),
            child: TweenAnimationBuilder<double>(
              tween: Tween(begin: 0, end: pct),
              duration: const Duration(milliseconds: 700),
              curve: Curves.easeOut,
              builder: (_, v, __) => LinearProgressIndicator(
                value: v,
                backgroundColor: AppColors.border,
                valueColor: AlwaysStoppedAnimation(color),
                minHeight: 8,
              ),
            ),
          ),
          const SizedBox(height: 8),
          Text('$occupied / $total slots occupied',
              style: GoogleFonts.rajdhani(fontSize: 12, color: AppColors.muted)),
        ],
      ),
    );
  }
}

// ─────────────────────────────────────────────────────────────
//  Section Label
// ─────────────────────────────────────────────────────────────

class _SectionLabel extends StatelessWidget {
  final String icon;
  final String title;
  final String sub;
  const _SectionLabel({required this.icon, required this.title, required this.sub});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Text('$icon  ${title.toUpperCase()}',
            style: GoogleFonts.rajdhani(
                fontSize: 13, fontWeight: FontWeight.w700,
                color: AppColors.muted, letterSpacing: 1.8)),
        if (sub.isNotEmpty) ...[
          const SizedBox(width: 10),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
            decoration: BoxDecoration(
              color: AppColors.surface,
              border: Border.all(color: AppColors.border),
              borderRadius: BorderRadius.circular(6),
            ),
            child: Text(sub,
                style: GoogleFonts.spaceMono(
                    fontSize: 9, color: AppColors.muted, letterSpacing: .8)),
          ),
        ],
        const SizedBox(width: 12),
        const Expanded(child: Divider(color: AppColors.border, thickness: .8)),
      ],
    );
  }
}

// ─────────────────────────────────────────────────────────────
//  Slot Grid
// ─────────────────────────────────────────────────────────────

class _SlotGrid extends StatelessWidget {
  final Map<int, SlotInfo> slots;
  final bool isPermanent;
  final Function(String) onRelease;  // ← Release callback
  const _SlotGrid({
    required this.slots,
    required this.isPermanent,
    required this.onRelease,
  });

  @override
  Widget build(BuildContext context) {
    final entries = slots.entries.toList()..sort((a, b) => a.key.compareTo(b.key));
    return LayoutBuilder(builder: (_, c) {
      final cols = c.maxWidth > 800 ? 4 : (c.maxWidth > 500 ? 2 : 2);
      return GridView.builder(
        shrinkWrap: true,
        physics: const NeverScrollableScrollPhysics(),
        gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
          crossAxisCount: cols,
          crossAxisSpacing: 14,
          mainAxisSpacing: 14,
          childAspectRatio: 1.35,
        ),
        itemCount: entries.length,
        itemBuilder: (_, i) => _SlotCard(
          slotNum: entries[i].key,
          info: entries[i].value,
          isPermanent: isPermanent,
          onRelease: onRelease,
        ),
      );
    });
  }
}

class _SlotCard extends StatefulWidget {
  final int slotNum;
  final SlotInfo info;
  final bool isPermanent;
  final Function(String) onRelease;  // ← Release callback
  const _SlotCard({
    required this.slotNum,
    required this.info,
    required this.isPermanent,
    required this.onRelease,
  });

  @override
  State<_SlotCard> createState() => _SlotCardState();
}

class _SlotCardState extends State<_SlotCard> {
  bool _hover = false;

  @override
  Widget build(BuildContext context) {
    final occ = widget.info.isOccupied;
    final borderColor = occ
        ? AppColors.red.withValues(alpha: _hover ? .7 : .35)
        : AppColors.green.withValues(alpha: _hover ? .7 : .3);
    final numColor = occ ? AppColors.red : AppColors.green;

    return MouseRegion(
      onEnter: (_) => setState(() => _hover = true),
      onExit: (_) => setState(() => _hover = false),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 200),
        padding: const EdgeInsets.all(18),
        decoration: BoxDecoration(
          color: AppColors.surface,
          border: Border.all(color: borderColor, width: .9),
          borderRadius: BorderRadius.circular(12),
          boxShadow: _hover && !occ
              ? [BoxShadow(color: AppColors.green.withValues(alpha: .08), blurRadius: 12)]
              : null,
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(
              widget.isPermanent ? 'PERMANENT · RFID' : 'VISITOR · WEB',
              style: GoogleFonts.spaceMono(fontSize: 9, color: AppColors.muted, letterSpacing: 1),
            ),
            const SizedBox(height: 8),
            Text(
              widget.slotNum.toString().padLeft(2, '0'),
              style: GoogleFonts.spaceMono(
                  fontSize: 34, fontWeight: FontWeight.w700, color: numColor, height: 1),
            ),
            const SizedBox(height: 8),
            _Badge(occupied: occ),
            if (occ && (widget.info.uid != null || widget.info.name != null)) ...[
              const SizedBox(height: 6),
              Text(
                widget.info.uid ?? widget.info.name ?? '',
                style: GoogleFonts.spaceMono(fontSize: 9, color: AppColors.muted),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
              const SizedBox(height: 8),
              SizedBox(
                height: 28,
                child: Material(
                  color: Colors.transparent,
                  child: InkWell(
                    onTap: () {
                      final releaseId = widget.isPermanent
                          ? (widget.info.uid ?? '')
                          : (widget.info.bookingId ?? '');
                      if (releaseId.isNotEmpty) {
                        widget.onRelease(releaseId);
                      }
                    },
                    borderRadius: BorderRadius.circular(6),
                    child: Container(
                      decoration: BoxDecoration(
                        color: AppColors.red.withValues(alpha: .2),
                        border: Border.all(color: AppColors.red, width: .8),
                        borderRadius: BorderRadius.circular(6),
                      ),
                      alignment: Alignment.center,
                      child: Text(
                        'Release',
                        style: GoogleFonts.spaceMono(
                          fontSize: 8,
                          fontWeight: FontWeight.w700,
                          color: AppColors.red,
                          letterSpacing: .5,
                        ),
                      ),
                    ),
                  ),
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _Badge extends StatelessWidget {
  final bool occupied;
  const _Badge({required this.occupied});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
      decoration: BoxDecoration(
        color: occupied ? AppColors.red.withValues(alpha: .12) : AppColors.green.withValues(alpha: .12),
        borderRadius: BorderRadius.circular(6),
      ),
      child: Text(
        occupied ? 'OCCUPIED' : 'FREE',
        style: GoogleFonts.spaceMono(
          fontSize: 10,
          fontWeight: FontWeight.w700,
          color: occupied ? AppColors.red : AppColors.green,
          letterSpacing: .5,
        ),
      ),
    );
  }
}

// ─────────────────────────────────────────────────────────────
//  Booking Panel
// ─────────────────────────────────────────────────────────────

class _BookingPanel extends StatelessWidget {
  final TextEditingController controller;
  final bool loading;
  final bool disabled;
  final VoidCallback onBook;

  const _BookingPanel({
    required this.controller,
    required this.loading,
    required this.disabled,
    required this.onBook,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(24),
      decoration: BoxDecoration(
        color: AppColors.surface,
        border: Border.all(color: AppColors.border, width: .8),
        borderRadius: BorderRadius.circular(12),
      ),
      child: LayoutBuilder(builder: (_, c) {
        final wide = c.maxWidth > 500;
        final nameField = Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('YOUR NAME  (optional)',
                style: GoogleFonts.spaceMono(
                    fontSize: 10, color: AppColors.muted, letterSpacing: 1.2)),
            const SizedBox(height: 8),
            TextField(
              controller: controller,
              style: GoogleFonts.rajdhani(fontSize: 16, color: AppColors.text),
              decoration: const InputDecoration(hintText: 'e.g. Raj Kumar'),
            ),
          ],
        );
        final bookBtn = SizedBox(
          height: 48,
          child: ElevatedButton.icon(
            onPressed: (loading || disabled) ? null : onBook,
            icon: loading
                ? const SizedBox(
                    width: 16, height: 16,
                    child: CircularProgressIndicator(strokeWidth: 1.8, color: Colors.black))
                : const Text('⚡', style: TextStyle(fontSize: 15)),
            label: Text(
              disabled ? 'ALL FULL' : 'BOOK NOW',
              style: GoogleFonts.rajdhani(
                  fontWeight: FontWeight.w800, fontSize: 15, letterSpacing: 1),
            ),
          ),
        );

        return wide
            ? Row(
                crossAxisAlignment: CrossAxisAlignment.end,
                children: [
                  Expanded(child: nameField),
                  const SizedBox(width: 16),
                  bookBtn,
                ],
              )
            : Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [nameField, const SizedBox(height: 16), bookBtn],
              );
      }),
    );
  }
}

// ─────────────────────────────────────────────────────────────
//  Active Bookings
// ─────────────────────────────────────────────────────────────

class _ActiveBookings extends StatelessWidget {
  final Map<int, SlotInfo> visitor;
  final Future<void> Function(String) onRelease;

  const _ActiveBookings({required this.visitor, required this.onRelease});

  @override
  Widget build(BuildContext context) {
    final active = visitor.entries
        .where((e) => e.value.isOccupied && e.value.bookingId != null)
        .toList()
      ..sort((a, b) => a.key.compareTo(b.key));

    if (active.isEmpty) {
      return Container(
        padding: const EdgeInsets.all(32),
        decoration: BoxDecoration(
          color: AppColors.surface,
          border: Border.all(color: AppColors.border, width: .8),
          borderRadius: BorderRadius.circular(12),
        ),
        child: Center(
          child: Text('No active visitor bookings',
              style: GoogleFonts.rajdhani(color: AppColors.muted, fontSize: 15)),
        ),
      );
    }

    return Column(
      children: active.map((e) => _BookingRow(
        slotNum: e.key,
        info: e.value,
        onRelease: onRelease,
      )).toList(),
    );
  }
}

class _BookingRow extends StatelessWidget {
  final int slotNum;
  final SlotInfo info;
  final Future<void> Function(String) onRelease;

  const _BookingRow({
    required this.slotNum,
    required this.info,
    required this.onRelease,
  });

  @override
  Widget build(BuildContext context) {
    String timeStr = '—';
    if (info.bookedAt != null) {
      try {
        final dt = DateTime.parse(info.bookedAt!).toLocal();
        timeStr =
            '${dt.hour.toString().padLeft(2, '0')}:${dt.minute.toString().padLeft(2, '0')}';
      } catch (_) {}
    }

    return Container(
      margin: const EdgeInsets.only(bottom: 10),
      padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 16),
      decoration: BoxDecoration(
        color: AppColors.surface,
        border: Border.all(color: AppColors.border, width: .8),
        borderRadius: BorderRadius.circular(12),
      ),
      child: LayoutBuilder(builder: (_, c) {
        final wide = c.maxWidth > 480;
        final info_row = Wrap(
          spacing: 24,
          runSpacing: 10,
          children: [
            _BookingField(label: 'SLOT', value: slotNum.toString()),
            _BookingField(label: 'NAME', value: info.name ?? 'Guest'),
            _BookingField(label: 'BOOKING ID', value: info.bookingId ?? '—'),
            _BookingField(label: 'TIME', value: timeStr),
          ],
        );
        final releaseBtn = TextButton(
          onPressed: () => onRelease(info.bookingId!),
          style: TextButton.styleFrom(
            foregroundColor: AppColors.red,
            backgroundColor: AppColors.red.withValues(alpha: .08),
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(8),
              side: BorderSide(color: AppColors.red.withValues(alpha: .3)),
            ),
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
          ),
          child: Text('Release',
              style: GoogleFonts.rajdhani(
                  fontWeight: FontWeight.w700, fontSize: 13, letterSpacing: .5)),
        );

        return wide
            ? Row(
                children: [
                  Expanded(child: info_row),
                  const SizedBox(width: 16),
                  releaseBtn,
                ],
              )
            : Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  info_row,
                  const SizedBox(height: 12),
                  releaseBtn,
                ],
              );
      }),
    );
  }
}

class _BookingField extends StatelessWidget {
  final String label;
  final String value;
  const _BookingField({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(label,
            style: GoogleFonts.spaceMono(
                fontSize: 9, color: AppColors.muted, letterSpacing: 1.1)),
        const SizedBox(height: 3),
        Text(value,
            style: GoogleFonts.rajdhani(
                fontSize: 15, color: AppColors.text, fontWeight: FontWeight.w600)),
      ],
    );
  }
}

// ─────────────────────────────────────────────────────────────
//  Offline Banner
// ─────────────────────────────────────────────────────────────

class _OfflineBanner extends StatelessWidget {
  final VoidCallback onRetry;
  const _OfflineBanner({required this.onRetry});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Container(
        padding: const EdgeInsets.all(36),
        decoration: BoxDecoration(
          color: AppColors.surface,
          border: Border.all(color: AppColors.red.withValues(alpha: .3)),
          borderRadius: BorderRadius.circular(16),
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Text('⚠', style: TextStyle(fontSize: 42)),
            const SizedBox(height: 16),
            Text('Cannot reach backend server',
                style: GoogleFonts.rajdhani(
                    fontSize: 20, fontWeight: FontWeight.w700, color: AppColors.text)),
            const SizedBox(height: 6),
            Text('Make sure Node.js server is running on port 3000',
                style: GoogleFonts.rajdhani(fontSize: 14, color: AppColors.muted)),
            const SizedBox(height: 20),
            ElevatedButton.icon(
              onPressed: onRetry,
              icon: const Icon(Icons.refresh_rounded, size: 16),
              label: const Text('Retry Connection'),
            ),
          ],
        ),
      ),
    );
  }
}
