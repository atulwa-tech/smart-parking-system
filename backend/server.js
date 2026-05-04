// ============================================================
//  Smart Parking System — Node.js Backend
//  server.js
// ============================================================

const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
const { v4: uuidv4 } = require('uuid');

const app = express();
const PORT = process.env.PORT || 3000;

// ── Middleware ────────────────────────────────────────────
app.use(cors());
app.use(bodyParser.json());

// Log all incoming requests
app.use((req, res, next) => {
  console.log(`[${new Date().toLocaleTimeString()}] ${req.method} ${req.path} - from ${req.ip}`);
  if (req.body && Object.keys(req.body).length > 0) {
    console.log(`  Body:`, JSON.stringify(req.body));
  }
  next();
});

// ── In-Memory Database ────────────────────────────────────

// Parking slots: { slotId -> { type, status, uid, bookingId, name, bookedAt } }
const slots = {
  permanent: {
    1: { status: 'available', uid: null, bookingId: null, name: null, bookedAt: null },
    2: { status: 'available', uid: null, bookingId: null, name: null, bookedAt: null },
    3: { status: 'available', uid: null, bookingId: null, name: null, bookedAt: null },
    4: { status: 'available', uid: null, bookingId: null, name: null, bookedAt: null },
  },
  visitor: {
    1: { status: 'available', uid: null, bookingId: null, name: null, bookedAt: null },
    2: { status: 'available', uid: null, bookingId: null, name: null, bookedAt: null },
    3: { status: 'available', uid: null, bookingId: null, name: null, bookedAt: null },
    4: { status: 'available', uid: null, bookingId: null, name: null, bookedAt: null },
  },
};

// Track active sessions: { bookingId -> { uid, slotType, slotNum, timestamp } }
const activeSessions = {};

// ── Gate Control (for ESP32) ──────────────────────────────
let gateCommand = {
  open: false,
  reason: '',
  timestamp: null,
  sentCount: 0,  // Track how many times gate signal was sent
};

// ── Utility Functions ──────────────────────────────────────

function getStats() {
  const permStats = Object.values(slots.permanent).reduce(
    (acc, slot) => ({
      free: acc.free + (slot.status === 'available' ? 1 : 0),
      occupied: acc.occupied + (slot.status === 'occupied' ? 1 : 0),
    }),
    { free: 0, occupied: 0 }
  );

  const visitStats = Object.values(slots.visitor).reduce(
    (acc, slot) => ({
      free: acc.free + (slot.status === 'available' ? 1 : 0),
      occupied: acc.occupied + (slot.status === 'occupied' ? 1 : 0),
    }),
    { free: 0, occupied: 0 }
  );

  return {
    permanentFree: permStats.free,
    permanentOccupied: permStats.occupied,
    visitorFree: visitStats.free,
    visitorOccupied: visitStats.occupied,
    totalFree: permStats.free + visitStats.free,
  };
}

function findAvailableSlot(type) {
  const slotGroup = slots[type];
  for (const [slotNum, slotData] of Object.entries(slotGroup)) {
    if (slotData.status === 'available') {
      return parseInt(slotNum);
    }
  }
  return null;
}

// ── Routes ────────────────────────────────────────────────

// GET /slots — Return all slots + stats
app.get('/slots', (req, res) => {
  res.json({
    slots,
    stats: getStats(),
  });
});

// GET /gate — ESP32 polls this to check if gate should open
app.get('/gate', (req, res) => {
  const response = {
    open: gateCommand.open,
    reason: gateCommand.reason,
  };

  // Send gate signal 3 times to ensure ESP32 catches it
  if (gateCommand.open) {
    gateCommand.sentCount++;
    console.log(`[GATE] Signal sent (${gateCommand.sentCount}/3): ${gateCommand.reason}`);
    
    // Reset after 3 sends (approximately 6 seconds = 3 polls × 2 seconds)
    if (gateCommand.sentCount >= 3) {
      gateCommand = {
        open: false,
        reason: '',
        timestamp: null,
        sentCount: 0,
      };
    }
  }

  res.json(response);
});

// POST /rfid — Handle RFID card scan from ESP32
// Expects: { uid: "1A2B3C4D" }
// Returns: { success, action, slot, bookingId }
// ANY card will be assigned to a permanent slot first, then visitor slot
app.post('/rfid', (req, res) => {
  const { uid } = req.body;

  if (!uid) {
    return res.status(400).json({ success: false, error: 'UID required' });
  }

  const upperUID = uid.toUpperCase();

  // Check if this UID is already parked (exit scenario)
  for (const [bookingId, session] of Object.entries(activeSessions)) {
    if (session.uid === upperUID) {
      // Release the slot
      const { slotType, slotNum } = session;
      slots[slotType][slotNum].status = 'available';
      slots[slotType][slotNum].uid = null;
      slots[slotType][slotNum].bookingId = null;
      slots[slotType][slotNum].name = null;
      slots[slotType][slotNum].bookedAt = null;

      delete activeSessions[bookingId];

      console.log(`[EXIT] UID ${uid} released ${slotType} slot ${slotNum}`);
      return res.json({
        success: true,
        action: 'exit',
        bookingId,
      });
    }
  }

  // Try to assign to permanent slot first (any card)
  let availableSlot = findAvailableSlot('permanent');
  let slotType = 'permanent';

  // If no permanent slots, try visitor slots
  if (!availableSlot) {
    availableSlot = findAvailableSlot('visitor');
    slotType = 'visitor';
  }

  // If no slots available at all
  if (!availableSlot) {
    return res.json({
      success: false,
      error: 'No slots available',
    });
  }

  // Book the slot
  const bookingId = uuidv4();
  const now = new Date().toISOString();

  slots[slotType][availableSlot] = {
    status: 'occupied',
    uid: upperUID,
    bookingId,
    name: `Car (${upperUID.substring(0, 8)})`,
    bookedAt: now,
  };

  activeSessions[bookingId] = {
    uid: upperUID,
    slotType: slotType,
    slotNum: availableSlot,
    timestamp: now,
  };

  console.log(
    `[ENTRY] UID ${uid} booked ${slotType} slot ${availableSlot}, BookingID: ${bookingId}`
  );

  res.json({
    success: true,
    action: 'entry',
    slot: availableSlot,
    bookingId,
  });
});

// POST /book — Book a visitor slot from dashboard
// Expects: { name: "John Doe" }
// Returns: { success, bookingId, slot }
app.post('/book', (req, res) => {
  const { name } = req.body;

  const availableSlot = findAvailableSlot('visitor');

  if (!availableSlot) {
    return res.json({
      success: false,
      error: 'No visitor slots available',
    });
  }

  const bookingId = uuidv4();
  const now = new Date().toISOString();

  slots.visitor[availableSlot] = {
    status: 'occupied',
    uid: null,
    bookingId,
    name: name || 'Guest',
    bookedAt: now,
  };

  activeSessions[bookingId] = {
    uid: null,
    slotType: 'visitor',
    slotNum: availableSlot,
    timestamp: now,
  };

  // ✓ TRIGGER GATE OPENING FOR VISITOR SLOT BOOKING
  gateCommand = {
    open: true,
    reason: `Visitor slot ${availableSlot} booked by ${name || 'Guest'}`,
    timestamp: now,
    sentCount: 0,
  };

  console.log(
    `[DASHBOARD BOOK] ${name || 'Guest'} booked visitor slot ${availableSlot}, BookingID: ${bookingId}`
  );

  res.json({
    success: true,
    bookingId,
    slot: availableSlot,
  });
});

// POST /release — Release visitor slot from dashboard
// Expects: { bookingId: "uuid" }
// Returns: { success, slot }
app.post('/release', (req, res) => {
  const { bookingId } = req.body;

  const session = activeSessions[bookingId];

  if (!session) {
    return res.json({
      success: false,
      error: 'Booking not found',
    });
  }

  const { slotType, slotNum } = session;

  slots[slotType][slotNum] = {
    status: 'available',
    uid: null,
    bookingId: null,
    name: null,
    bookedAt: null,
  };

  delete activeSessions[bookingId];

  console.log(
    `[DASHBOARD RELEASE] ${slotType} slot ${slotNum} released, BookingID: ${bookingId}`
  );

  res.json({
    success: true,
    slot: slotNum,
  });
});

// POST /release-permanent — Release a permanent slot from dashboard
// Expects: { uid: "PERM_SLOT_1" }
// Returns: { success, slot }
app.post('/release-permanent', (req, res) => {
  const { uid } = req.body;

  if (!uid) {
    return res.json({
      success: false,
      error: 'UID not provided',
    });
  }

  // Find permanent slot with matching UID
  let slotNum = null;
  for (const [num, slotData] of Object.entries(slots.permanent)) {
    if (slotData.uid === uid) {
      slotNum = num;
      break;
    }
  }

  if (slotNum === null) {
    return res.json({
      success: false,
      error: 'Permanent slot not found',
    });
  }

  slots.permanent[slotNum] = {
    status: 'available',
    uid: null,
    bookingId: null,
    name: null,
    bookedAt: null,
  };

  console.log(
    `[DASHBOARD RELEASE] Permanent slot ${slotNum} released, UID: ${uid}`
  );

  res.json({
    success: true,
    slot: parseInt(slotNum),
  });
});

// GET /health — Health check
app.get('/health', (req, res) => {
  res.json({
    status: 'ok',
    timestamp: new Date().toISOString(),
  });
});

// ── Start Server ──────────────────────────────────────────

app.listen(PORT, '0.0.0.0', () => {
  console.log(
    `\n=== Smart Parking Backend ===\n Server running at http://0.0.0.0:${PORT}\n`
  );
  console.log('✓ System ready - any RFID card will be accepted\n');
});
