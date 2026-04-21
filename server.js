// ============================================================
// Smart Parking System — Node.js + Express Backend
// ============================================================
// Slots 1–4  : Permanent (RFID)
// Slots 5–8  : Visitor   (Web booking)
// ============================================================

const express    = require('express');
const cors       = require('cors');
const bodyParser = require('body-parser');

const app  = express();
const PORT = 3000;

app.use(cors());
app.use(bodyParser.json());

// ─── In-Memory Data Store ────────────────────────────────────

// Map RFID card UIDs → fixed permanent slot numbers
const rfidRegistry = {
  "A1B2C3D4": 1,
  "E5F6G7H8": 2,
  "I9J0K1L2": 3,
  "M3N4O5P6": 4
};

// Parking slot state
// status  : "available" | "occupied"
// uid     : card UID or booking ID when occupied
const slots = {
  permanent: {
    1: { status: "available", uid: null },
    2: { status: "available", uid: null },
    3: { status: "available", uid: null },
    4: { status: "available", uid: null }
  },
  visitor: {
    5: { status: "available", bookingId: null, bookedAt: null },
    6: { status: "available", bookingId: null, bookedAt: null },
    7: { status: "available", bookingId: null, bookedAt: null },
    8: { status: "available", bookingId: null, bookedAt: null }
  }
};

// ─── Helper Functions ─────────────────────────────────────────

function getStats() {
  const permOccupied  = Object.values(slots.permanent).filter(s => s.status === "occupied").length;
  const visitOccupied = Object.values(slots.visitor).filter(s => s.status === "occupied").length;
  return {
    permanentTotal  : 4,
    permanentOccupied: permOccupied,
    permanentFree   : 4 - permOccupied,
    visitorTotal    : 4,
    visitorOccupied : visitOccupied,
    visitorFree     : 4 - visitOccupied,
    totalFree       : (4 - permOccupied) + (4 - visitOccupied)
  };
}

// ─── Routes ──────────────────────────────────────────────────

/**
 * GET /slots
 * Returns full slot status + summary stats
 */
app.get('/slots', (req, res) => {
  res.json({
    success: true,
    slots,
    stats: getStats()
  });
});

/**
 * POST /rfid
 * Body: { "uid": "A1B2C3D4" }
 * Returns assigned slot number for permanent parking
 */
app.post('/rfid', (req, res) => {
  const { uid } = req.body;

  if (!uid) {
    return res.status(400).json({ success: false, message: "UID is required" });
  }

  const assignedSlot = rfidRegistry[uid.toUpperCase()];

  if (!assignedSlot) {
    return res.status(403).json({ success: false, message: "Unregistered RFID card" });
  }

  const slot = slots.permanent[assignedSlot];

  // Toggle: if currently occupied by this card → release; otherwise → occupy
  if (slot.status === "occupied" && slot.uid === uid.toUpperCase()) {
    slot.status = "available";
    slot.uid    = null;
    return res.json({
      success  : true,
      action   : "exit",
      slot     : assignedSlot,
      message  : `Slot ${assignedSlot} released. Goodbye!`
    });
  }

  if (slot.status === "occupied") {
    return res.status(409).json({
      success : false,
      message : `Slot ${assignedSlot} is already occupied by another vehicle`
    });
  }

  // Occupy the slot
  slot.status = "occupied";
  slot.uid    = uid.toUpperCase();

  return res.json({
    success : true,
    action  : "entry",
    slot    : assignedSlot,
    message : `Welcome! Your slot is ${assignedSlot}`
  });
});

/**
 * POST /book
 * Body: {} (optional: { "name": "John" } for identification)
 * Allocates the next available visitor slot
 */
app.post('/book', (req, res) => {
  const { name } = req.body || {};

  // Find first available visitor slot
  const availableSlotNum = Object.keys(slots.visitor).find(
    num => slots.visitor[num].status === "available"
  );

  if (!availableSlotNum) {
    return res.status(409).json({
      success : false,
      message : "No visitor slots available. All 4 visitor slots are occupied."
    });
  }

  const bookingId = "BK-" + Date.now();
  slots.visitor[availableSlotNum].status    = "occupied";
  slots.visitor[availableSlotNum].bookingId = bookingId;
  slots.visitor[availableSlotNum].bookedAt  = new Date().toISOString();
  slots.visitor[availableSlotNum].name      = name || "Guest";

  return res.json({
    success   : true,
    slot      : parseInt(availableSlotNum),
    bookingId,
    name      : name || "Guest",
    message   : `Slot ${availableSlotNum} booked successfully!`
  });
});

/**
 * POST /release
 * Body: { "bookingId": "BK-123456" }
 * Releases a visitor slot
 */
app.post('/release', (req, res) => {
  const { bookingId } = req.body;

  if (!bookingId) {
    return res.status(400).json({ success: false, message: "bookingId is required" });
  }

  const slotNum = Object.keys(slots.visitor).find(
    num => slots.visitor[num].bookingId === bookingId
  );

  if (!slotNum) {
    return res.status(404).json({ success: false, message: "Booking ID not found" });
  }

  slots.visitor[slotNum].status    = "available";
  slots.visitor[slotNum].bookingId = null;
  slots.visitor[slotNum].bookedAt  = null;
  slots.visitor[slotNum].name      = null;

  return res.json({
    success : true,
    slot    : parseInt(slotNum),
    message : `Slot ${slotNum} has been released.`
  });
});

// ─── Start Server ─────────────────────────────────────────────

app.listen(PORT, () => {
  console.log(`✅ Smart Parking Backend running at http://localhost:${PORT}`);
  console.log(`   GET  /slots   → View all slots`);
  console.log(`   POST /rfid    → RFID card check`);
  console.log(`   POST /book    → Book visitor slot`);
  console.log(`   POST /release → Release visitor slot`);
});
