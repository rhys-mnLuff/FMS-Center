/* =====================================================================
   FMS - Flood Monitoring System
   BASE STATION  (Base_Code.ino)               firmware 2026-09-26, packet v2
   Board: ATmega328P (Arduino Uno or Nano). Two SoftwareSerial ports with
   an RX on D2 rule out Mega/Leonardo, so this is a 328P board.

   !! The packet format changed. Reflash the base AND every transmitter. !!

   WIRING (every pin this sketch touches)
   ---------------------------------------------------------------------
   D0, D1  USB serial (Serial Monitor, 9600 baud)
   D2      <- HC-12 TXD     (SoftwareSerial RX)
   D3      -> HC-12 RXD     (SoftwareSerial TX)
   D10     <- SIM7600 TXD   (SoftwareSerial RX)
   D11     -> SIM7600 RXD   (SoftwareSerial TX)
   5V/GND  HC-12 power. SIM7600 powered as before. Common GND.
   Every other pin unused.

   HOW THE TWO RADIOS SHARE ONE CPU
   SoftwareSerial can only listen to ONE port at a time. The base now listens
   to the HC-12 almost all the time. It only switches to the SIM7600 for a
   fraction of a second to check for "STATUS" texts (every 20 s). While it
   waits on the modem (SMS / upload delays), it goes back to listening to
   the HC-12, so packets keep arriving during an SMS or upload.
   ===================================================================== */

/* =============================================================
   FIXES APPLIED (read before flashing):

   1. COMPILE ERROR FIXED: API_KEY_NODE_2 and API_KEY_NODE_3 were
      used in sendToThingSpeak() but never declared. Added below
      as placeholders, fill in your real ThingSpeak (or new app)
      write keys per node.

   2. SOIL THRESHOLD DIRECTION FIXED: your own calibration data
      (this session, Aug 31 2026) showed DRY averages ~875 raw,
      SATURATED averages ~385 raw - lower raw value means wetter.
      The original code fired on "soilMoisture > 200", which is
      true for BOTH dry and saturated soil, so it would have
      alerted almost constantly. Changed to "< SOIL_ALERT_THRESHOLD"
      and moved the threshold to 450 (between your saturated mean
      of 385 and field capacity, which you haven't finished
      calibrating yet). TREAT 450 AS A PLACEHOLDER - replace it
      with a real number once field capacity calibration is done,
      same as moisture_alert_logic.ino does with TRIGGER_PCT.

   3. SERVER ENDPOINT IS NOW CONFIGURABLE: pulled the ThingSpeak
      URL apart into SERVER_HOST / SERVER_PATH constants. Once the
      new custom app is live, you only change those two lines,
      nothing else in sendToThingSpeak() needs to change AS LONG AS
      the new app accepts the same query-string format ThingSpeak
      does (api_key, field1, field2, field3, lat, long). See the
      web app brief for why that matters.

   4. WATER_ALERT_THRESHOLD direction NOT changed - that sensor
      hasn't been calibrated the way soil moisture has, so I don't
      know its polarity yet. Verify direction before trusting it.

   SEP 26 2026 UPDATE (relay + battery + rain):
   5. HC-12 RECEIVING FIXED: the old loop listened to the SIM7600 for
      1 s, switched to the HC-12 for a few microseconds, then switched
      back. listen() empties the receive buffer, so packets were almost
      never received. The HC-12 is now the default listener.
   6. WATCHDOG FIXED: GPS (2 s) + upload (5 s) + GPS (2 s) + SMS (5 s)
      ran without wdt_reset(), so the 8 s watchdog rebooted the board
      before a flood SMS could go out. All modem waits now reset it.
   7. STATUS texts are now stored on the SIM and polled (AT+CNMI=2,1)
      instead of pushed, because the base no longer sits listening to
      the modem. Read messages are deleted after each check.
   8. MODEM BAUD: the SIM module starts at 115200 after every power-up
      (your bench sketches only changed it per session). The base now
      switches it to 9600 itself at boot, checks it answers, and prints
      SIM card / signal / network status. Type m any time to recheck.
   9. SMS and uploads now wait for the modem's answer and print SENT /
      FAILED. A failed flood SMS no longer starts the 1-hour cooldown;
      it retries every 60 s while the flood lasts.
  10. FLOOD RISK LEVEL (RISK_LEVELS): NORMAL / WATCH / WARNING / FLOOD from
      rain + soil + water, per the Findings Brief. Shown on each line,
      uploaded as field8, and in STATUS replies. Display only: it does
      not change when SMS alerts fire.
   ============================================================= */

#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <stdarg.h>

// ============== FEATURE SWITCHES (1 = on, 0 = off if it breaks) ==============
#define DEMO_MODE              1   // Only a starting guess for the OFFLINE timer. After two packets the base
                                   // learns each node's own send rate: ~2 s -> OFFLINE after 10 s,
                                   // ~10 s -> OFFLINE after 10 min. So it no longer has to match the nodes.
#define FORCE_RELAY_FROM_NODE  0   // relay test: ignore packets from this node ID that arrive
                                   // direct (hops=0), forcing them through a relay. 0 = off.
#define LOW_BATT_SMS           1   // 1: one SMS when a node is CRITICAL + discharging (max 1 per 12 h per node).
#define STATUS_SMS_ENABLED     1   // 1: reply to "STATUS" texts (checked every 20 s).
#define USE_GNSS_QUERY         0   // 0: map link always uses SITE_LAT_LONG below (no 2 s radio blackout).
                                   // 1: old behaviour - ask the modem for GNSS before every upload/SMS;
                                   //    link shows 0.0,0.0 until the modem has a fix.
#define SMS_MAP_LINK           0   // 0: SMS show the location as plain coordinates ("Loc: 14.5333,121.0000").
                                   // 1: Google Maps link. Philippine networks BLOCK texts containing links
                                   //    (tested 16 Oct 2026 on Smart: link texts rejected or never delivered).
#define SMS_PLAIN_LANGUAGE     1   // 1: farmer-friendly SMS in words (FLOODING / SATURATED / heavy rain + what to do).
                                   // 0: old SMS with raw numbers.
#define THINGSPEAK_ENABLED     1   // 0: no dashboard uploads (SMS testing). 1: upload to ThingSpeak again.
#define TS_EXTRA_FIELDS        1   // 1: also upload field4 rain, field5 batt %, field6 batt mV,
                                   //    field7 10-min avg mA. Fields 1-3 unchanged.
#define TS_MIN_INTERVAL_MS 20000UL // min gap between uploads per node (ThingSpeak free tier rejects
                                   // updates closer than 15 s). 0 = upload every packet like before.
#define ALERT_RULES_BRIEF      1   // 1: Sensor Findings Brief (17 Sep) rules - water > 30 or soil <= 436,
                                   //    held for 3 readings in a row, cleared only at soil > 470 and
                                   //    water <= 30. 0: old rule - soil < 450 or water > 200, one reading.
                                   // Either way: same SMS text, same 1-per-hour flood cooldown.
#define RISK_LEVELS            1   // 1: show a flood-risk level (NORMAL/WATCH/WARNING/FLOOD) on every line,
                                   //    upload it as ThingSpeak field8 (0-3), add it to STATUS replies.
                                   //    Display only - never sends an SMS by itself.

// Old single-reading rule (ALERT_RULES_BRIEF 0). SOIL_ALERT_THRESHOLD also sets the health index.
const int SOIL_ALERT_THRESHOLD = 450;   // PLACEHOLDER - see note 2 above. Was 200 with wrong direction.
const int WATER_ALERT_THRESHOLD = 200;  // Direction VERIFIED (Findings Brief 17 Sep: idle 0, rises when wet).
                                        // Value not re-tuned: the brief suggests > 30 held for 3 readings.

// Findings Brief rule (ALERT_RULES_BRIEF 1). Soil 436 = saturated mean 385.4 + 2 SD (measured,
// 1 of 3 test rounds). Water 30 and soil 470 are chosen buffers, not measured values.
const int BRIEF_WATER_ALERT_ABOVE = 30;
const int BRIEF_SOIL_ALERT_AT_OR_BELOW = 436;
const int BRIEF_SOIL_CLEAR_ABOVE = 470;        // hysteresis: soil must come back past this to clear
const int BRIEF_WATER_CLEAR_AT_OR_BELOW = 30;
const uint8_t ALERT_CONFIRM_READINGS = 3;      // debounce: consecutive readings before alerting

// Risk level (RISK_LEVELS 1), from the Findings Brief level table:
//   FLOOD   water > 30
//   WARNING soil <= 436 (saturated)
//   WATCH   raining (rain < 980), or soil 437-551 (wetter than ideal)
//   NORMAL  otherwise
// Soil 960+ = probe out of soil: soil is ignored and rain + water decide.
const int RISK_SOIL_WATCH_BELOW = 552;         // ESTIMATED (brief: typical clay-loam data, not your tests)
const int RISK_SOIL_FAULT_FROM = 960;

const int SOIL_WILTING_POINT = 800;
const int SOIL_OPTIMAL_POINT = 400;

const int NUM_PHONES = 1;
const char* const FARMER_PHONES[NUM_PHONES] = { "+639761388436" };   // farmer's phone (international format, no spaces)

unsigned long lastSmsTime = 0;
const unsigned long SMS_COOLDOWN_MS = 3600000;   // max 1 flood SMS per hour, shared by all nodes
const char CELLULAR_APN[] = "internet";

// --- MULTI-NODE API KEYS ---
// Put your unique write keys here for each node's channel.
const char API_KEY_NODE_1[] = "O3JGLAD7IX4P67IE";
const char API_KEY_NODE_2[] = "REPLACE_WITH_NODE_2_KEY"; // was missing, added as placeholder
const char API_KEY_NODE_3[] = "REPLACE_WITH_NODE_3_KEY"; // was missing, added as placeholder

// --- SERVER ENDPOINT (swap these two lines once the custom app is live) ---
const char SERVER_HOST[] = "generations-unix-come-cartridges.trycloudflare.com";
const char SERVER_PATH[] = "/update";

// --- SITE NAMES shown in every SMS (node 1, 2, 3). Keep each under ~25 characters. ---
const char SITE_NAME_1[] PROGMEM = "Bench Testing Version";
const char SITE_NAME_2[] PROGMEM = "Node 2";
const char SITE_NAME_3[] PROGMEM = "Node 3";

// --- LOCATION (provisioned; used for every map link) ---
const char SITE_LAT_LONG[] = "14.5333,121.0000";

// --- NEW SETTINGS ---
#define BATTERY_CAPACITY_MAH  2200               // same as the transmitters, for runtime estimates
#define LOW_BATT_SMS_COOLDOWN_MS 43200000UL      // 12 h per node, separate from flood cooldown
#define NODE_OFFLINE_MS  (DEMO_MODE ? 10000UL : 600000UL)
#define POWER_LINE_EVERY_MS 60000UL              // print each node's power line once a minute
#define STATUS_POLL_MS   20000UL                 // how often to check the SIM for STATUS texts
#define SMS_RETRY_MS     60000UL                 // after a failed SMS, wait this long before trying again
#define MODEM_RECHECK_MS 60000UL                 // if the modem stops answering, look for it this often
#define DUP_CACHE_SIZE   16
#define DUP_EXPIRY_MS    30000UL
#define MAX_NODES        6

SoftwareSerial hc12(2, 3);
SoftwareSerial sim7600(10, 11);

// --- FLAGS BYTE (same as Sensor_Code.ino) ---
#define FLAG_RAIN_MASK      0x03  // bits 0-1: 0 NONE, 1 LIGHT, 2 HEAVY, 3 no reading
#define FLAG_CHARGING       0x04
#define FLAG_DISCHARGING    0x08
#define FLAG_BATT_LOW       0x10
#define FLAG_BATT_CRITICAL  0x20
#define FLAG_NO_BATT_DATA   0x40
#define FLAG_BOOT           0x80

// NodeInfo.state bits
#define ST_OFFLINE          0x01
#define ST_ALERT_PENDING    0x02
#define ST_UPLOAD_PENDING   0x04
#define ST_BATT_SMS_PENDING 0x08
#define ST_BATT_SMS_SENT    0x10
#define ST_UPLOADED         0x20
#define ST_POWER_PRINTED    0x40
#define ST_ALERTED          0x80

// Alert state for one packet
#define AL_NORMAL   0
#define AL_WATCHING 1   // over the trigger, not yet confirmed - silent
#define AL_ALERTED  2   // confirmed; flood SMS allowed (subject to the 1 h cooldown)
#define AL_CLEARED  3   // just dropped back through the bands

#define RISK_NORMAL  0
#define RISK_WATCH   1
#define RISK_WARNING 2
#define RISK_FLOOD   3

struct Packet {
  uint8_t origin, seq, hops, flags;
  int16_t soil, water, rain, battPct, battMv, avgMa;
};

struct SeenEntry {
  uint8_t origin, seq;        // origin 0 = empty slot
  unsigned long t;
};

struct NodeInfo {
  uint8_t id;                 // 0 = free slot
  uint8_t state, hops, flags, lastSeq, alertCount;
  int16_t soil, water, rain, pct, mv, ma;
  int16_t alertSoil, alertWater;
  uint16_t lost;              // packets missed (gaps in seq)
  uint16_t intervalDs;        // learned time between this node's packets, in 0.1 s (0 = not yet)
  unsigned long lastHeard, lastUpload, lastBattSms, lastPowerPrint;
};

int latestSoil = 0;
int latestWater = 0;
char latestNode[4] = "N/A";
uint8_t latestFlags = FLAG_RAIN_MASK | FLAG_NO_BATT_DATA;
int latestPct = -1;
char latLong[24] = "0.0,0.0";

NodeInfo nodes[MAX_NODES];
SeenEntry seen[DUP_CACHE_SIZE];
uint8_t seenNext = 0;

char rxBuf[64];
uint8_t rxLen = 0;
bool rxActive = false;
unsigned long lastRxByteMs = 0;

char smsBuf[161];             // every SMS body is built here: 160 chars max
bool verbose = false;
unsigned long lastOfflineCheck = 0, lastStatusPoll = 0, lastCooldownNote = 0, lastOldFwNote = 0;
unsigned int statOk = 0, statDup = 0, statBad = 0, statForced = 0;

// --- WATCHDOG SAFETY (runs before setup) ---
// After a watchdog reset the chip keeps the watchdog on with a 16 ms timeout; switch it off
// before anything else runs so a board without a bootloader can't get stuck resetting.
uint8_t resetFlags __attribute__((section(".noinit")));
void earlyWatchdogOff(void) __attribute__((naked, used, section(".init3")));
void earlyWatchdogOff(void) {
  resetFlags = MCUSR;
  MCUSR = 0;
  wdt_disable();
}

// =====================================================================
//  PACKET FORMAT (identical copy lives in Sensor_Code.ino - keep in sync)
// =====================================================================

int8_t hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// body = everything between '<' and '>'. Checks checksum, field count and ranges.
bool parsePacket(char *body, Packet &p) {
  char *star = strchr(body, '*');
  if (!star) return false;
  int8_t hi = hexDigit(star[1]), lo = hexDigit(star[2]);
  if (hi < 0 || lo < 0 || star[3] != '\0') return false;
  uint8_t ck = 0;
  for (char *c = body; c < star; c++) ck ^= (uint8_t)*c;
  if (ck != (uint8_t)((hi << 4) | lo)) return false;
  *star = '\0';

  long v[10];
  char *s = body;
  for (uint8_t i = 0; i < 10; i++) {
    char *end;
    v[i] = strtol(s, &end, 10);
    if (end == s) return false;                   // empty / non-numeric field
    if (i < 9) {
      if (*end != ',') return false;              // too few fields
      s = end + 1;
    } else if (*end != '\0') {
      return false;                               // too many fields
    }
  }
  if (v[0] < 1 || v[0] > 254 || v[1] < 0 || v[1] > 255 || v[2] < 0 || v[2] > 15) return false;
  if (v[3] < 0 || v[3] > 1023 || v[4] < 0 || v[4] > 1023 || v[5] < -1 || v[5] > 1023) return false;
  if (v[6] < -1 || v[6] > 100 || v[7] < -1 || v[7] > 30000) return false;
  if (v[8] < -9999 || v[8] > 9999 || v[9] < 0 || v[9] > 255) return false;

  p.origin = v[0]; p.seq = v[1]; p.hops = v[2];
  p.soil = v[3]; p.water = v[4]; p.rain = v[5];
  p.battPct = v[6]; p.battMv = v[7]; p.avgMa = v[8]; p.flags = v[9];
  return true;
}

// True if (origin, seq) was seen in the last DUP_EXPIRY_MS. Otherwise remembers it.
bool seenBefore(uint8_t origin, uint8_t seq) {
  unsigned long now = millis();
  for (uint8_t i = 0; i < DUP_CACHE_SIZE; i++) {
    if (seen[i].origin == origin && seen[i].seq == seq && now - seen[i].t < DUP_EXPIRY_MS) return true;
  }
  uint8_t slot = seenNext;
  for (uint8_t i = 0; i < DUP_CACHE_SIZE; i++) {
    if (seen[i].origin == 0 || now - seen[i].t >= DUP_EXPIRY_MS) { slot = i; break; }
  }
  if (slot == seenNext) seenNext = (seenNext + 1) % DUP_CACHE_SIZE;
  seen[slot].origin = origin;
  seen[slot].seq = seq;
  seen[slot].t = now;
  return false;
}

// =====================================================================
//  PRINT HELPERS
// =====================================================================

void printSigned(int v) {
  if (v >= 0) Serial.print('+');
  Serial.print(v);
}

void printVolts(int mv) {
  int cv = (mv + 5) / 10;
  Serial.print(cv / 100);
  Serial.print('.');
  if (cv % 100 < 10) Serial.print('0');
  Serial.print(cv % 100);
  Serial.print('V');
}

const __FlashStringHelper *rainWord(uint8_t flags) {
  switch (flags & FLAG_RAIN_MASK) {
    case 0:  return F("NONE");
    case 1:  return F("LIGHT");
    case 2:  return F("HEAVY");
    default: return F("--");
  }
}

const __FlashStringHelper *chargeWord(uint8_t flags) {
  if (flags & FLAG_CHARGING) return F("charging");
  if (flags & FLAG_DISCHARGING) return F("discharging");
  return F("idle");
}

// Tenths of an hour left, or -1 when not discharging / no data.
long runtimeTenths(int pct, int avgMa, uint8_t flags) {
  if (pct < 0 || (flags & FLAG_NO_BATT_DATA) || !(flags & FLAG_DISCHARGING) || avgMa >= 0) return -1;
  return (long)pct * BATTERY_CAPACITY_MAH / (10L * -avgMa);
}

void printAgo(unsigned long ms) {
  unsigned long s = ms / 1000;
  if (s < 120) { Serial.print(s); Serial.print(F(" s")); }
  else { Serial.print(s / 60); Serial.print(F(" min")); }
}

// A node sending every ~2 s (demo) is OFFLINE after 10 s, one sending every ~10 s after 10 min.
// A node on a critical battery may be in battery saver (one packet a minute), so it gets the long timer.
unsigned long offlineTimeout(const NodeInfo &n) {
  if ((n.flags & FLAG_BATT_CRITICAL) && !(n.flags & FLAG_CHARGING)) return 600000UL;
  if (n.intervalDs) return n.intervalDs < 50 ? 10000UL : 600000UL;
  return NODE_OFFLINE_MS;                    // not learned yet: go by DEMO_MODE
}

void printOfflineTimeout(unsigned long ms) {
  if (ms >= 60000UL) { Serial.print(ms / 60000UL); Serial.print(F(" min")); }
  else { Serial.print(ms / 1000UL); Serial.print(F(" s")); }
}

uint8_t riskLevel(int soil, int water, uint8_t flags) {
  if (water > BRIEF_WATER_ALERT_ABOVE) return RISK_FLOOD;
  bool soilOk = soil < RISK_SOIL_FAULT_FROM;
  uint8_t rain = flags & FLAG_RAIN_MASK;
  bool raining = (rain == 1 || rain == 2);
  if (soilOk && soil <= BRIEF_SOIL_ALERT_AT_OR_BELOW) return RISK_WARNING;
  if (raining || (soilOk && soil < RISK_SOIL_WATCH_BELOW)) return RISK_WATCH;
  return RISK_NORMAL;
}

const __FlashStringHelper *riskWord(uint8_t level) {
  switch (level) {
    case RISK_FLOOD:   return F("FLOOD");
    case RISK_WARNING: return F("WARNING");
    case RISK_WATCH:   return F("WATCH");
    default:           return F("NORMAL");
  }
}

// One line per received packet, e.g.
// NODE 2 | hops 0 | soil 612 | water 45 | rain HEAVY | batt 87% charging
void printPacketLine(const Packet &p, uint8_t alert, uint8_t watchCount) {
  Serial.print(F("NODE "));      Serial.print(p.origin);
  Serial.print(F(" | hops "));   Serial.print(p.hops);
  Serial.print(F(" | soil "));   Serial.print(p.soil);
  Serial.print(F(" | water "));  Serial.print(p.water);
  Serial.print(F(" | rain "));   Serial.print(rainWord(p.flags));
  Serial.print(F(" | batt "));
  if (p.battPct < 0 || (p.flags & FLAG_NO_BATT_DATA)) {
    Serial.print(F("--"));
  } else {
    Serial.print(p.battPct);
    Serial.print(F("% "));
    Serial.print(chargeWord(p.flags));
    if (p.flags & FLAG_BATT_CRITICAL) Serial.print(F(" CRITICAL"));
    else if (p.flags & FLAG_BATT_LOW) Serial.print(F(" LOW"));
  }
  if (p.flags & FLAG_BOOT) Serial.print(F(" | just booted"));
#if RISK_LEVELS
  Serial.print(F(" | risk "));
  Serial.print(riskWord(riskLevel(p.soil, p.water, p.flags)));
#endif
  if (alert == AL_ALERTED) Serial.print(F("  << FLOOD LEVEL"));
  else if (alert == AL_CLEARED) Serial.print(F("  << alert cleared"));
  else if (alert == AL_WATCHING) {
    Serial.print(F("  << WATCHING "));
    Serial.print(watchCount);
    Serial.print('/');
    Serial.print(ALERT_CONFIRM_READINGS);
  }
  Serial.println();
}

// e.g. NODE 2 power | 3.89V | 10-min avg -58mA | est. 33.0h left
void printPowerLine(const NodeInfo &n) {
  Serial.print(F("NODE "));
  Serial.print(n.id);
  Serial.print(F(" power | "));
  if (n.pct < 0 || (n.flags & FLAG_NO_BATT_DATA)) {
    Serial.println(F("no battery data (INA219 missing?)"));
    return;
  }
  printVolts(n.mv);
  Serial.print(F(" | 10-min avg "));
  printSigned(n.ma);
  Serial.print(F("mA | "));
  long h = runtimeTenths(n.pct, n.ma, n.flags);
  if (h >= 0) {
    Serial.print(F("est. "));
    Serial.print(h / 10);
    Serial.print('.');
    Serial.print(h % 10);
    Serial.println(F("h left"));
  } else if (n.flags & FLAG_CHARGING) {
    Serial.println(F("runtime -- (charging)"));
  } else {
    Serial.println(F("runtime --"));
  }
}

int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

// =====================================================================
//  NODE TABLE
// =====================================================================

NodeInfo *findNode(uint8_t id) {
  for (uint8_t i = 0; i < MAX_NODES; i++) if (nodes[i].id == id) return &nodes[i];
  // New node: take a free slot, or the one silent the longest (never one with an alert waiting)
  NodeInfo *pick = 0;
  for (uint8_t i = 0; i < MAX_NODES; i++) {
    if (nodes[i].id == 0) { pick = &nodes[i]; break; }
    if (nodes[i].state & ST_ALERT_PENDING) continue;
    if (!pick || millis() - nodes[i].lastHeard > millis() - pick->lastHeard) pick = &nodes[i];
  }
  if (!pick) return 0;
  memset(pick, 0, sizeof(NodeInfo));
  pick->id = id;
  return pick;
}

// Only used if the node table is full: still honour a flood reading.
bool overflowAlertPending = false;
uint8_t overflowNode = 0;
int overflowSoil = 0, overflowWater = 0;
uint8_t overflowFlags = 0;
void triggerAlertLater(const Packet &p) {
  overflowAlertPending = true;
  overflowNode = p.origin;
  overflowSoil = p.soil;
  overflowWater = p.water;
  overflowFlags = p.flags;
}

bool alertPendingAny() {
  if (overflowAlertPending) return true;
  for (uint8_t i = 0; i < MAX_NODES; i++) if (nodes[i].id && (nodes[i].state & ST_ALERT_PENDING)) return true;
  return false;
}

// =====================================================================
//  RADIO (HC-12)
// =====================================================================

void reportBadPacket(const char *body) {
  statBad++;
  // Old 3-field format "<node,soil,water>" = a transmitter still on the old firmware
  if (!strchr(body, '*')) {
    uint8_t commas = 0;
    for (const char *c = body; *c; c++) if (*c == ',') commas++;
    if (commas == 2 && (lastOldFwNote == 0 || millis() - lastOldFwNote > 10000UL)) {
      lastOldFwNote = millis();
      Serial.print(F("OLD FIRMWARE packet <"));
      Serial.print(body);
      Serial.println(F("> - reflash that transmitter!"));
      return;
    }
  }
  if (verbose) {
    Serial.print(F("  (bad packet dropped: <"));
    Serial.print(body);
    Serial.println(F(">)"));
  }
}

// One step of the NORMAL -> WATCHING -> ALERTED state machine for a node.
uint8_t alertStep(NodeInfo *n, const Packet &p) {
#if ALERT_RULES_BRIEF
  bool trigger = p.water > BRIEF_WATER_ALERT_ABOVE || p.soil <= BRIEF_SOIL_ALERT_AT_OR_BELOW;
  if (!n) return trigger ? AL_ALERTED : AL_NORMAL;       // node table full: fall back to one reading
  if (n->state & ST_ALERTED) {
    if (p.water <= BRIEF_WATER_CLEAR_AT_OR_BELOW && p.soil > BRIEF_SOIL_CLEAR_ABOVE) {
      n->state &= ~ST_ALERTED;
      n->alertCount = 0;
      return AL_CLEARED;
    }
    return AL_ALERTED;                                    // still inside the bands: stays alerted
  }
  if (!trigger) { n->alertCount = 0; return AL_NORMAL; }
  if (++n->alertCount >= ALERT_CONFIRM_READINGS) {
    n->state |= ST_ALERTED;
    return AL_ALERTED;
  }
  return AL_WATCHING;
#else
  (void)n;
  // Same alert rule as before, checked on every packet
  return (p.soil < SOIL_ALERT_THRESHOLD || p.water > WATER_ALERT_THRESHOLD) ? AL_ALERTED : AL_NORMAL;
#endif
}

// Called for every complete <...>. Prints it and records what needs doing.
// It never talks to the modem, so it is safe to call from inside modem waits.
void handleIncoming(char *body) {
  Packet p;
  char copy[64];
  strncpy(copy, body, sizeof(copy) - 1);
  copy[sizeof(copy) - 1] = '\0';
  if (!parsePacket(body, p)) { reportBadPacket(copy); return; }

#if FORCE_RELAY_FROM_NODE != 0
  if (p.origin == FORCE_RELAY_FROM_NODE && p.hops == 0) {
    statForced++;
    Serial.print(F("  (NODE "));
    Serial.print(p.origin);
    Serial.println(F(" direct packet ignored - FORCE_RELAY test)"));
    return;   // not cached, so the relayed copy is still accepted
  }
#endif

  if (seenBefore(p.origin, p.seq)) {
    statDup++;
    if (verbose) {
      Serial.print(F("  (duplicate NODE "));
      Serial.print(p.origin);
      Serial.print(F(" #"));
      Serial.print(p.seq);
      Serial.print(F(" via hops "));
      Serial.print(p.hops);
      Serial.println(F(" ignored)"));
    }
    return;
  }
  statOk++;

  NodeInfo *n = findNode(p.origin);
  uint8_t alert = alertStep(n, p);
  bool flood = (alert == AL_ALERTED);
  printPacketLine(p, alert, n ? n->alertCount : 0);

  latestSoil = p.soil;
  latestWater = p.water;
  utoa(p.origin, latestNode, 10);
  latestFlags = p.flags;
  latestPct = p.battPct;

  if (!n) {
    if (flood) triggerAlertLater(p);
    return;
  }
  unsigned long now = millis();

  if (n->state & ST_OFFLINE) {
    Serial.print(F("NODE "));
    Serial.print(n->id);
    Serial.println(F(" BACK ONLINE"));
    n->state &= ~ST_OFFLINE;
  }
  if (n->lastHeard != 0 && !(p.flags & FLAG_BOOT)) {
    uint8_t gap = (uint8_t)(p.seq - n->lastSeq - 1);
    if (gap < 50) n->lost += gap;
  }
  if (n->lastHeard != 0) {                     // learn how often this node sends
    unsigned long gap = now - n->lastHeard;
    if (gap < 300000UL) {
      uint16_t ds = gap / 100;
      n->intervalDs = n->intervalDs ? (uint16_t)((n->intervalDs * 3UL + ds) / 4) : ds;
    }
  }
  n->lastSeq = p.seq;
  n->hops = p.hops;
  n->flags = p.flags;
  n->soil = p.soil;
  n->water = p.water;
  n->rain = p.rain;
  n->pct = p.battPct;
  n->mv = p.battMv;
  n->ma = p.avgMa;
  n->lastHeard = now | 1;     // never 0 (0 = never heard)
  n->state |= ST_UPLOAD_PENDING;

  if (flood) {
    n->alertSoil = p.soil;
    n->alertWater = p.water;
    n->state |= ST_ALERT_PENDING;
  }

#if LOW_BATT_SMS
  if ((p.flags & FLAG_BATT_CRITICAL) && (p.flags & FLAG_DISCHARGING) && !(p.flags & FLAG_NO_BATT_DATA) &&
      (!(n->state & ST_BATT_SMS_SENT) || now - n->lastBattSms >= LOW_BATT_SMS_COOLDOWN_MS)) {
    n->state |= ST_BATT_SMS_PENDING;
  }
#endif

  if (!(n->state & ST_POWER_PRINTED) || now - n->lastPowerPrint >= POWER_LINE_EVERY_MS) {
    n->state |= ST_POWER_PRINTED;
    n->lastPowerPrint = now;
    printPowerLine(*n);
  }
}

void pollRadio() {
  while (hc12.available()) {
    char c = hc12.read();
    lastRxByteMs = millis();
    if (c == '<') { rxActive = true; rxLen = 0; continue; }
    if (!rxActive) continue;
    if (c == '>') {
      rxBuf[rxLen] = '\0';
      rxActive = false;
      handleIncoming(rxBuf);
      continue;
    }
    if (c == '\r' || c == '\n' || rxLen >= sizeof(rxBuf) - 1) {   // broken packet
      rxBuf[rxLen] = '\0';
      rxActive = false;
      reportBadPacket(rxBuf);
      continue;
    }
    rxBuf[rxLen++] = c;
  }
  if (rxActive && millis() - lastRxByteMs > 100) rxActive = false;   // cut off mid-air
}

bool radioQuiet() {
  return !rxActive && millis() - lastRxByteMs > 50;
}

// Replaces delay() while we wait on the modem: the modem's replies were never
// read anyway, so we listen to the HC-12 instead and keep the watchdog fed.
void radioWait(unsigned long ms) {
  hc12.listen();
  unsigned long start = millis();
  while (millis() - start < ms) {
    wdt_reset();
    pollRadio();
  }
}

// =====================================================================
//  MODEM HELPERS: send a command, read the answer
// =====================================================================

#define MODEM_TIMEOUT (-1)
#define MODEM_ERROR   0
#define MODEM_FOUND   1

char modemLine[48];            // last line the modem sent, for printing results
bool modemAlive = false;
unsigned long lastModemCheck = 0, smsFailedAt = 0;

// Listens to the modem until a line starts with `want` (FOUND), a line contains "ERROR"
// (ERROR), or timeoutMs passes (TIMEOUT). A want of ">" matches the SMS text prompt.
// The HC-12 is not heard meanwhile, so these waits are kept short or rare.
int8_t modemWaitFor(PGM_P want, unsigned long timeoutMs) {
  sim7600.listen();
  bool prompt = pgm_read_byte(want) == '>';
  uint8_t len = 0;
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    wdt_reset();
    if (!sim7600.available()) continue;
    char c = sim7600.read();
    if (prompt && c == '>') return MODEM_FOUND;
    if (c == '\r') continue;
    if (c != '\n') {
      if (len < sizeof(modemLine) - 1) modemLine[len++] = c;
      continue;
    }
    modemLine[len] = '\0';
    len = 0;
    if (modemLine[0] == '\0') continue;
    if (!prompt && strncmp_P(modemLine, want, strlen_P(want)) == 0) return MODEM_FOUND;
    if (strstr_P(modemLine, PSTR("ERROR"))) return MODEM_ERROR;
  }
  strcpy_P(modemLine, PSTR("no reply"));   // never print a half line (could be garbage or a phone number)
  return MODEM_TIMEOUT;
}

// Sends one AT command and waits for its answer.
int8_t modemCommand(const __FlashStringHelper *cmd, PGM_P want, unsigned long timeoutMs) {
  pollRadio();                       // keep radio bytes before listen() empties the buffer
  sim7600.listen();
  while (sim7600.available()) sim7600.read();
  sim7600.println(cmd);
  return modemWaitFor(want, timeoutMs);
}

// The SIM module powers up at 115200 baud (AT+IPR isn't saved), which SoftwareSerial can't
// receive reliably. Try 9600; if silent, send AT+IPR=9600 at 115200 (sending is fine at that
// speed) and switch back. True once the modem answers OK at 9600.
bool modemConnect(uint8_t attempts) {
  for (uint8_t i = 0; i < attempts; i++) {
    sim7600.begin(9600);
    if (modemCommand(F("AT"), PSTR("OK"), 800) == MODEM_FOUND) return true;
    sim7600.begin(115200);
    sim7600.print(F("AT+IPR=9600\r\n"));
    delay(200);
    wdt_reset();
  }
  sim7600.begin(9600);
  return false;
}

// Settings the modem forgets on every power-up (same commands the old setup sent).
void modemConfigure() {
  modemCommand(F("AT+CMGF=1"), PSTR("OK"), 1000);
#if STATUS_SMS_ENABLED
  modemCommand(F("AT+CPMS=\"SM\",\"SM\",\"SM\""), PSTR("OK"), 2000);
  modemCommand(F("AT+CNMI=2,1,0,0,0"), PSTR("OK"), 1000);   // store incoming texts; pollStatusSms() reads them
#else
  modemCommand(F("AT+CNMI=1,2,0,0,0"), PSTR("OK"), 1000);
#endif
  modemCommand(F("AT+CGNSSPWR=1"), PSTR("OK"), 1000);
  modemCommand(F("AT+HTTPTERM"), PSTR("OK"), 1000);         // close an HTTP session left over from a reset
  hc12.listen();
}

int8_t regState(const __FlashStringHelper *cmd, PGM_P want) {
  if (modemCommand(cmd, want, 1000) != MODEM_FOUND) return -1;
  char *comma = strchr(modemLine, ',');
  return comma ? atoi(comma + 1) : -1;
}

// One line: is the modem there, is the SIM ready, how is the signal, is it on the network.
void printModemStatus() {
  Serial.print(F("SIM module: "));
  if (modemCommand(F("AT"), PSTR("OK"), 1000) != MODEM_FOUND) {
    modemAlive = false;
    Serial.println(F("NOT RESPONDING - check its power (2 A peaks), D10/D11 wiring and the D11 divider."));
    Serial.println(F("            Alerts still print here, but SMS and uploads can't go out."));
    hc12.listen();
    return;
  }
  modemAlive = true;
  Serial.print(F("OK | SIM card "));
  if (modemCommand(F("AT+CPIN?"), PSTR("+CPIN:"), 2000) == MODEM_FOUND && strstr_P(modemLine, PSTR("READY"))) {
    Serial.print(F("ready"));
  } else {
    Serial.print(F("NOT READY ("));
    Serial.print(modemLine);
    Serial.print(')');
  }
  Serial.print(F(" | signal "));
  if (modemCommand(F("AT+CSQ"), PSTR("+CSQ:"), 1000) == MODEM_FOUND) {
    int rssi = atoi(modemLine + 5);
    if (rssi == 99) {
      Serial.print(F("NONE"));
    } else {
      Serial.print(rssi);
      Serial.print(F("/31 "));
      Serial.print(rssi < 10 ? F("weak") : rssi < 15 ? F("ok") : rssi < 20 ? F("good") : F("excellent"));
    }
  } else {
    Serial.print('?');
  }
  Serial.print(F(" | network "));
  int8_t st = regState(F("AT+CEREG?"), PSTR("+CEREG:"));          // LTE
  if (st != 1 && st != 5) st = regState(F("AT+CREG?"), PSTR("+CREG:"));
  if (st == 1) Serial.println(F("registered"));
  else if (st == 5) Serial.println(F("registered (roaming)"));
  else if (st == 2) Serial.println(F("searching... (can take a minute after power-up)"));
  else if (st == 3) Serial.println(F("DENIED (SIM or plan problem)"));
  else Serial.println(F("NOT registered"));
  hc12.listen();
}

// After a failed SMS/upload: is the modem still there at all?
void checkModemAfterFailure() {
  if (modemCommand(F("AT"), PSTR("OK"), 1000) != MODEM_FOUND) {
    modemAlive = false;
    lastModemCheck = millis();
    Serial.println(F("SIM module stopped answering - will look for it every 60 s"));
  }
  hc12.listen();
}

bool smsRetryReady() {
  return smsFailedAt == 0 || millis() - smsFailedAt >= SMS_RETRY_MS;
}

// =====================================================================
//  MODEM: SMS, GPS, THINGSPEAK (same AT commands as before)
// =====================================================================

#if USE_GNSS_QUERY
void updateGPS() {
  pollRadio();
  sim7600.listen();
  sim7600.println("AT+CGNSSINFO");
  unsigned long timeout = millis();
  while(millis() - timeout < 2000) {
    wdt_reset();
    if(sim7600.available()) {
      String gpsRaw = sim7600.readString();
      if(gpsRaw.indexOf(",N,") > 0 || gpsRaw.indexOf(",S,") > 0) {
        // TODO: this still hardcodes a placeholder instead of parsing gpsRaw.
        strcpy(latLong, SITE_LAT_LONG); // Placeholder for prototype
      }
    }
  }
  hc12.listen();
}
#endif

// Same AT+CMGS sequence as before, but it waits for the modem's "> " prompt before the text
// and for "+CMGS" (network accepted it) after. Returns how many phones it reached.
// Phone numbers are not printed, so they never show up on camera.
uint8_t sendSMS(const char *message) {
  if (strlen(message) > 160) {   // can't happen with smsBuf[161], but never send a split SMS
    Serial.println(F("SMS too long - not sent"));
    return 0;
  }
  uint8_t sent = 0;
  modemCommand(F("AT+CMGF=1"), PSTR("OK"), 1000);   // text mode again, in case the modem rebooted
  for (int i = 0; i < NUM_PHONES; i++) {
    pollRadio();          // grab any radio bytes before listen() empties the buffer
    sim7600.listen();
    while(sim7600.available()) { sim7600.read(); }
    sim7600.print(F("AT+CMGS=\""));
    sim7600.print(FARMER_PHONES[i]);
    sim7600.println(F("\""));
    Serial.print(F("    SMS to phone "));
    Serial.print(i + 1);
    if (modemWaitFor(PSTR(">"), 5000) != MODEM_FOUND) {
      sim7600.write(27);                             // ESC: cancel, in case it's half-way
      Serial.print(F(": FAILED, no prompt ("));
      Serial.print(modemLine);
      Serial.println(')');
      continue;
    }
    sim7600.print(message);
    delay(100);
    sim7600.write(26);
    if (modemWaitFor(PSTR("+CMGS:"), 20000) == MODEM_FOUND) {
      sent++;
      Serial.println(F(": sent OK"));
    } else {
      Serial.print(F(": FAILED ("));
      Serial.print(modemLine);
      Serial.println(')');
    }
  }
  hc12.listen();
  if (!sent) checkModemAfterFailure();
  return sent;
}

// =====================================================================
//  PLAIN-LANGUAGE SMS (SMS_PLAIN_LANGUAGE 1)
//  Words come from the same bands the alerts use (Sensor Findings Brief):
//    water  > 30         FLOODING          (water is touching the sensor)
//    soil   <= 436       SATURATED         (ground can't take more water)
//           437-551      wet,  552-699 normal,  700-828 dry,  829-959 very dry,  960+ no reading
//    rain   flag         no rain / light rain / heavy rain / no reading
//  Every message is built to fit one 160-character SMS and contains no links.
// =====================================================================

uint8_t smsLen = 0;

void smsStart() { smsBuf[0] = '\0'; smsLen = 0; }

// Appends a formatted line. %S takes a PROGMEM string.
void smsAdd(PGM_P fmt, ...) {
  if (smsLen >= sizeof(smsBuf) - 1) return;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf_P(smsBuf + smsLen, sizeof(smsBuf) - smsLen, fmt, ap);
  va_end(ap);
  if (n > 0) smsLen = ((unsigned)(smsLen + n) < sizeof(smsBuf) - 1) ? smsLen + n : sizeof(smsBuf) - 1;
}

// Appends only if the whole piece still fits (used for optional extra detail).
void smsAddIfFits(PGM_P fmt, ...) {
  char tmp[48];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf_P(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  if (n > 0 && n < (int)sizeof(tmp) && smsLen + n <= 160) { strcpy(smsBuf + smsLen, tmp); smsLen += n; }
}

void siteName(uint8_t id, char *out, uint8_t size) {
  PGM_P name = 0;
  if (id == 1) name = SITE_NAME_1;
  else if (id == 2) name = SITE_NAME_2;
  else if (id == 3) name = SITE_NAME_3;
  if (name) { strncpy_P(out, name, size - 1); out[size - 1] = '\0'; }
  else snprintf_P(out, size, PSTR("Node %u"), id);
}

PGM_P soilWords(int soil) {
  if (soil >= RISK_SOIL_FAULT_FROM) return PSTR("no reading");
  if (soil <= BRIEF_SOIL_ALERT_AT_OR_BELOW) return PSTR("SATURATED");
  if (soil < RISK_SOIL_WATCH_BELOW) return PSTR("wet");
  if (soil < 700) return PSTR("normal");
  if (soil < 829) return PSTR("dry");
  return PSTR("very dry");
}

PGM_P rainWords(uint8_t flags) {
  switch (flags & FLAG_RAIN_MASK) {
    case 0:  return PSTR("no rain");
    case 1:  return PSTR("light rain");
    case 2:  return PSTR("heavy rain");
    default: return PSTR("no reading");
  }
}

PGM_P riskWords(uint8_t level) {
  switch (level) {
    case RISK_FLOOD:   return PSTR("FLOODING NOW");
    case RISK_WARNING: return PSTR("WARNING - flood possible");
    case RISK_WATCH:   return PSTR("WATCH - keep an eye out");
    default:           return PSTR("NORMAL - all OK");
  }
}

void smsAddLocation() {
#if SMS_MAP_LINK
  smsAdd(PSTR("Map: https://maps.google.com/?q=%s"), latLong);
#else
  smsAdd(PSTR("Loc: %s"), latLong);
#endif
}

void buildFloodSms(uint8_t node, int soil, int water, uint8_t flags) {
  char site[28];
  siteName(node, site, sizeof(site));
  bool waterThere = water > BRIEF_WATER_ALERT_ABOVE;
  smsStart();
  smsAdd(waterThere ? PSTR("FMS FLOOD ALERT\n") : PSTR("FMS FLOOD WARNING\n"));
  smsAdd(PSTR("%s\n"), site);
  smsAdd(PSTR("Water: %S\n"), waterThere ? PSTR("FLOODING") : PSTR("none yet"));
  smsAdd(PSTR("Soil: %S\n"), soilWords(soil));
  smsAdd(PSTR("Rain: %S\n"), rainWords(flags));
  smsAdd(waterThere ? PSTR("Act now: move to higher ground\n") : PSTR("Flood likely soon. Get ready\n"));
  smsAddLocation();
  smsAddIfFits(PSTR("\nRaw s%d w%d"), soil, water);
}

void buildStatusSms() {
  smsStart();
  smsAdd(PSTR("FMS STATUS\n"));
  if (latestNode[0] < '0' || latestNode[0] > '9') {     // nothing received yet
    smsAdd(PSTR("No sensor data received yet.\n"));
    smsAddLocation();
    return;
  }
  char site[28];
  siteName(atoi(latestNode), site, sizeof(site));
  smsAdd(PSTR("%s\n"), site);
  smsAdd(PSTR("Risk: %S\n"), riskWords(riskLevel(latestSoil, latestWater, latestFlags)));
  smsAdd(PSTR("Water: %S\n"), latestWater > BRIEF_WATER_ALERT_ABOVE ? PSTR("FLOODING") : PSTR("no water"));
  smsAdd(PSTR("Soil: %S\n"), soilWords(latestSoil));
  smsAdd(PSTR("Rain: %S\n"), rainWords(latestFlags));
  smsAddLocation();
  if (latestPct >= 0 && !(latestFlags & FLAG_NO_BATT_DATA)) {
    smsAddIfFits(PSTR("\nBattery: %d%%%S"), latestPct,
                 (latestFlags & FLAG_CHARGING) ? PSTR(" charging") : (latestFlags & FLAG_DISCHARGING) ? PSTR(" draining") : PSTR(""));
  } else {
    smsAddIfFits(PSTR("\nBattery: not measured"));
  }
}

void buildLowBatterySms(uint8_t node, int pct) {
  char site[28];
  siteName(node, site, sizeof(site));
  smsStart();
  smsAdd(PSTR("FMS BATTERY LOW\n"));
  smsAdd(PSTR("%s\n"), site);
  smsAdd(PSTR("Battery: %d%% and draining\n"), pct);
  smsAdd(PSTR("Sensor may stop soon. Check the solar panel is clean and in sun"));
}

void triggerAlert(uint8_t node, int soil, int water, uint8_t flags) {
  unsigned long currentMillis = millis();
  bool cooldownOver = (lastSmsTime == 0 || (currentMillis - lastSmsTime >= SMS_COOLDOWN_MS));
  if (cooldownOver && smsRetryReady()) {
#if USE_GNSS_QUERY
    updateGPS();
#endif
#if SMS_PLAIN_LANGUAGE
    buildFloodSms(node, soil, water, flags);
#else
    (void)flags;
    snprintf_P(smsBuf, sizeof(smsBuf),
#if SMS_MAP_LINK
               PSTR("FLOOD ALERT - Node %u!\nSoil: %d\nWater: %d\nLoc: https://maps.google.com/?q=%s"),
#else
               PSTR("FLOOD ALERT - Node %u!\nSoil: %d\nWater: %d\nLoc: %s"),
#endif
               node, soil, water, latLong);
#endif
    Serial.print(F(">>> FLOOD ALERT: sending SMS for NODE "));
    Serial.println(node);
    if (sendSMS(smsBuf)) {
      lastSmsTime = currentMillis;                 // cooldown only starts once it really went out
      smsFailedAt = 0;
      Serial.println(F(">>> FLOOD ALERT SMS sent. Next flood SMS allowed in 60 min."));
    } else {
      smsFailedAt = millis() | 1;
      Serial.println(F(">>> FLOOD ALERT SMS FAILED - retrying every 60 s while the flood lasts (m = modem status)"));
    }
  } else if (lastCooldownNote == 0 || currentMillis - lastCooldownNote >= 60000UL) {
    lastCooldownNote = currentMillis | 1;
    if (!cooldownOver) {
      Serial.print(F("    (flood SMS on cooldown - next allowed in "));
      Serial.print((SMS_COOLDOWN_MS - (currentMillis - lastSmsTime)) / 60000UL + 1);
      Serial.println(F(" min)"));
    } else {
      Serial.println(F("    (flood SMS failed earlier - retrying within 60 s)"));
    }
  }
}

void sendLowBatterySMS(NodeInfo &n) {
#if SMS_PLAIN_LANGUAGE
  buildLowBatterySms(n.id, n.pct);
#else
  snprintf_P(smsBuf, sizeof(smsBuf), PSTR("FMS: Site %u battery critical (%d%%). Check solar panel."),
             n.id, n.pct);
#endif
  Serial.print(F(">>> LOW BATTERY: sending SMS for NODE "));
  Serial.println(n.id);
  if (sendSMS(smsBuf)) {
    n.state |= ST_BATT_SMS_SENT;
    n.lastBattSms = millis();
    Serial.println(F(">>> LOW BATTERY SMS sent. Next one for this node in 12 h at the earliest."));
  } else {
    smsFailedAt = millis() | 1;                    // not marked sent: it tries again on a later packet
    Serial.println(F(">>> LOW BATTERY SMS FAILED - will retry"));
  }
}

int calculateSoilHealth(int soilMoisture) {
  if (soilMoisture >= SOIL_WILTING_POINT) return 0;
  if (soilMoisture <= SOIL_ALERT_THRESHOLD) return 20;
  int healthScore = map(soilMoisture, SOIL_WILTING_POINT, SOIL_OPTIMAL_POINT, 0, 100);
  return constrain(healthScore, 0, 100);
}

// --- DYNAMIC ROUTING, CONFIGURABLE SERVER ---
// Same AT sequence and URL as before, printed piece by piece instead of built in a String.
void sendToThingSpeak(const NodeInfo &n, int healthIndex) {
  const char *targetKey;

  if (n.id == 1) targetKey = API_KEY_NODE_1;
  else if (n.id == 2) targetKey = API_KEY_NODE_2;
  else if (n.id == 3) targetKey = API_KEY_NODE_3;
  else return; // Ignore unknown nodes

  pollRadio();
  sim7600.listen();
  sim7600.print(F("AT+CGDCONT=1,\"IP\",\""));
  sim7600.print(CELLULAR_APN);
  sim7600.println(F("\""));
  radioWait(1000);
  sim7600.println(F("AT+HTTPINIT"));
  radioWait(500);

  // Uses SERVER_HOST/SERVER_PATH so this can point at the new app later
  // by changing two constants at the top of the file, nothing here.
  sim7600.print(F("AT+HTTPPARA=\"URL\",\"http://"));
  sim7600.print(SERVER_HOST);
  sim7600.print(SERVER_PATH);
  sim7600.print(F("?api_key="));
  sim7600.print(targetKey);
  sim7600.print(F("&field1=")); sim7600.print(n.soil);
  sim7600.print(F("&field2=")); sim7600.print(n.water);
  sim7600.print(F("&field3=")); sim7600.print(healthIndex);
#if TS_EXTRA_FIELDS
  if (n.rain >= 0) { sim7600.print(F("&field4=")); sim7600.print(n.rain); }
  if (n.pct >= 0 && !(n.flags & FLAG_NO_BATT_DATA)) {
    sim7600.print(F("&field5=")); sim7600.print(n.pct);
    sim7600.print(F("&field6=")); sim7600.print(n.mv);
    sim7600.print(F("&field7=")); sim7600.print(n.ma);
  }
#endif

#if RISK_LEVELS
  sim7600.print(F("&field8=")); sim7600.print(riskLevel(n.soil, n.water, n.flags));
#endif

  // latLong is "lat,long" -> &lat=XX&long=YY
  const char *comma = strchr(latLong, ',');
  sim7600.print(F("&lat="));
  if (comma) sim7600.write((const uint8_t *)latLong, comma - latLong);
  sim7600.print(F("&long="));
  if (comma) sim7600.print(comma + 1);
  sim7600.println(F("\""));
  radioWait(1000);

  // Wait for the server's answer (up to 10 s) instead of a fixed 2 s: cellular requests often
  // take longer, and AT+HTTPTERM would cancel them. "+HTTPACTION: 0,200,<len>" = accepted.
  int8_t r = modemCommand(F("AT+HTTPACTION=0"), PSTR("+HTTPACTION:"), 10000);
  int code = -1;
  if (r == MODEM_FOUND) {
    char *c = strchr(modemLine, ',');
    if (c) code = atoi(c + 1);
  }
  Serial.print(F("    ThingSpeak NODE "));
  Serial.print(n.id);
  if (code == 200) {
    Serial.println(F(": uploaded"));
  } else if (code > 0) {
    Serial.print(F(": FAILED (HTTP "));
    Serial.print(code);
    Serial.println(code >= 600 ? F(" - no data connection? check APN and data plan)") : F(")"));
  } else {
    Serial.print(F(": FAILED ("));
    Serial.print(modemLine);
    Serial.println(')');
  }
  sim7600.println(F("AT+HTTPTERM"));
  radioWait(500);
  if (r == MODEM_TIMEOUT) checkModemAfterFailure();
  hc12.listen();
}

#if STATUS_SMS_ENABLED
void sendStatusReply() {
#if USE_GNSS_QUERY
  updateGPS();
#endif
#if SMS_PLAIN_LANGUAGE
  buildStatusSms();
#else
  char batt[6] = "--";
  if (latestPct >= 0 && !(latestFlags & FLAG_NO_BATT_DATA)) snprintf_P(batt, sizeof(batt), PSTR("%d%%"), latestPct);
  char rain[6];
  strncpy_P(rain, (const char *)rainWord(latestFlags), sizeof(rain) - 1);
  rain[sizeof(rain) - 1] = '\0';
#if RISK_LEVELS
  char risk[8];
  strncpy_P(risk, (const char *)riskWord(riskLevel(latestSoil, latestWater, latestFlags)), sizeof(risk) - 1);
  risk[sizeof(risk) - 1] = '\0';
  snprintf_P(smsBuf, sizeof(smsBuf),
#if SMS_MAP_LINK
             PSTR("FMS STATUS: Node %s\nRisk: %s\nSoil: %d\nWater: %d\nRain: %s\nBatt: %s\nMap: https://maps.google.com/?q=%s"),
#else
             PSTR("FMS STATUS: Node %s\nRisk: %s\nSoil: %d\nWater: %d\nRain: %s\nBatt: %s\nLoc: %s"),
#endif
             latestNode, risk, latestSoil, latestWater, rain, batt, latLong);
#else
  snprintf_P(smsBuf, sizeof(smsBuf),
#if SMS_MAP_LINK
             PSTR("FMS STATUS: Node %s\nSoil: %d\nWater: %d\nRain: %s\nBatt: %s\nMap: https://maps.google.com/?q=%s"),
#else
             PSTR("FMS STATUS: Node %s\nSoil: %d\nWater: %d\nRain: %s\nBatt: %s\nLoc: %s"),
#endif
             latestNode, latestSoil, latestWater, rain, batt, latLong);
#endif
#endif
  Serial.println(F(">>> STATUS requested by SMS - sending reply"));
  if (sendSMS(smsBuf)) {
    Serial.println(F(">>> STATUS reply sent"));
  } else {
    smsFailedAt = millis() | 1;
    Serial.println(F(">>> STATUS reply FAILED"));
  }
}

// Lists unread texts on the SIM. Any containing "STATUS" (any case) gets one reply.
void pollStatusSms() {
  pollRadio();
  sim7600.listen();
  while (sim7600.available()) sim7600.read();
  sim7600.println(F("AT+CMGL=\"REC UNREAD\""));

  static const char STATUS_WORD[] PROGMEM = "STATUS";
  bool gotMessage = false, wantStatus = false;
  uint8_t match = 0;
  char line[8];
  uint8_t lineLen = 0;
  unsigned long start = millis();
  while (millis() - start < 1500) {
    wdt_reset();
    if (!sim7600.available()) continue;
    char c = sim7600.read();
    char u = (c >= 'a' && c <= 'z') ? c - 32 : c;
    if (u == (char)pgm_read_byte(&STATUS_WORD[match])) {
      if (++match == 6) { wantStatus = true; match = 0; }
    } else {
      match = (u == 'S') ? 1 : 0;
    }
    if (c == '\n') {
      line[lineLen] = '\0';
      if (strncmp_P(line, PSTR("+CMGL:"), 6) == 0) gotMessage = true;
      if (strcmp_P(line, PSTR("OK")) == 0 || strncmp_P(line, PSTR("ERROR"), 5) == 0 ||
          strncmp_P(line, PSTR("+CMS ER"), 7) == 0) break;
      lineLen = 0;
    } else if (c != '\r' && lineLen < sizeof(line) - 1) {
      line[lineLen++] = c;
    }
  }
  hc12.listen();

  if (gotMessage) {
    sim7600.println(F("AT+CMGD=1,1"));   // delete all READ messages so the SIM never fills up
    radioWait(500);
  }
  if (wantStatus) sendStatusReply();
}
#endif

// =====================================================================
//  WORK QUEUE: flood alerts first, then uploads, then low-battery SMS
// =====================================================================

void processNodes() {
  // 1. Flood alerts always go first
  if (overflowAlertPending) {
    overflowAlertPending = false;
    triggerAlert(overflowNode, overflowSoil, overflowWater, overflowFlags);
    return;
  }
  for (uint8_t i = 0; i < MAX_NODES; i++) {
    NodeInfo &n = nodes[i];
    if (n.id && (n.state & ST_ALERT_PENDING)) {
      n.state &= ~ST_ALERT_PENDING;
      triggerAlert(n.id, n.alertSoil, n.alertWater, n.flags);
      return;
    }
  }
  // 2. One upload per pass, so a new alert never waits behind several uploads
#if THINGSPEAK_ENABLED
  for (uint8_t i = 0; i < MAX_NODES; i++) {
    NodeInfo &n = nodes[i];
    if (!modemAlive) break;                      // keep data pending until the modem is back
    if (!n.id || !(n.state & ST_UPLOAD_PENDING)) continue;
#if TS_MIN_INTERVAL_MS > 0
    if ((n.state & ST_UPLOADED) && millis() - n.lastUpload < TS_MIN_INTERVAL_MS) continue;
#endif
    n.state &= ~ST_UPLOAD_PENDING;
    n.state |= ST_UPLOADED;
    n.lastUpload = millis();
#if USE_GNSS_QUERY
    updateGPS();
#endif
    // --- ROUTE DATA TO CORRECT DASHBOARD/APP ---
    sendToThingSpeak(n, calculateSoilHealth(n.soil));
    return;
  }
#endif
  // 3. Low-battery SMS only when no flood alert is waiting
  for (uint8_t i = 0; i < MAX_NODES; i++) {
    NodeInfo &n = nodes[i];
    if (n.id && (n.state & ST_BATT_SMS_PENDING) && modemAlive && smsRetryReady()) {
      n.state &= ~ST_BATT_SMS_PENDING;
      sendLowBatterySMS(n);
      return;
    }
  }
}

void checkOfflineNodes() {
  if (millis() - lastOfflineCheck < 1000) return;
  lastOfflineCheck = millis();
  for (uint8_t i = 0; i < MAX_NODES; i++) {
    NodeInfo &n = nodes[i];
    if (!n.id || (n.state & ST_OFFLINE)) continue;
    if (millis() - n.lastHeard > offlineTimeout(n)) {
      n.state |= ST_OFFLINE;
      Serial.print(F("NODE "));
      Serial.print(n.id);
      Serial.print(F(" OFFLINE (nothing for "));
      printOfflineTimeout(offlineTimeout(n));
      Serial.println(F(")"));
    }
  }
}

// =====================================================================
//  SERIAL COMMANDS
// =====================================================================

void printHelp() {
  Serial.println(F("COMMANDS  n node table | m modem status | v show/hide duplicates | h help"));
}

void printNodeTable() {
  bool any = false;
  for (uint8_t i = 0; i < MAX_NODES; i++) {
    NodeInfo &n = nodes[i];
    if (!n.id) continue;
    any = true;
    Serial.print(F("NODE "));
    Serial.print(n.id);
    Serial.print((n.state & ST_OFFLINE) ? F(" OFFLINE | last ") : F(" | last "));
    printAgo(millis() - n.lastHeard);
    Serial.print(F(" ago | hops "));   Serial.print(n.hops);
    Serial.print(F(" | soil "));       Serial.print(n.soil);
    Serial.print(F(" | water "));      Serial.print(n.water);
    Serial.print(F(" | rain "));       Serial.print(n.rain);
    Serial.print(' ');                 Serial.print(rainWord(n.flags));
#if RISK_LEVELS
    Serial.print(F(" | risk "));       Serial.print(riskWord(riskLevel(n.soil, n.water, n.flags)));
#endif
    Serial.print(F(" | lost "));       Serial.println(n.lost);
    printPowerLine(n);
  }
  if (!any) Serial.println(F("no nodes heard yet"));
  Serial.print(F("packets ok "));   Serial.print(statOk);
  Serial.print(F(" | duplicates ")); Serial.print(statDup);
  Serial.print(F(" | bad "));       Serial.print(statBad);
  Serial.print(F(" | forced-relay ignored ")); Serial.print(statForced);
  Serial.print(F(" | free RAM "));  Serial.print(freeRam());
  Serial.println(F(" B"));
}

void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (c == 'n') printNodeTable();
    else if (c == 'm') printModemStatus();
    else if (c == 'v') { verbose = !verbose; Serial.println(verbose ? F("VERBOSE on") : F("VERBOSE off")); }
    else if (c == 'h' || c == '?') printHelp();
  }
}

void printStartupWarnings() {
  for (int i = 0; i < NUM_PHONES; i++) {
    if (strcmp_P(FARMER_PHONES[i], PSTR("+1234567890")) == 0) {
      Serial.println(F("WARNING: FARMER_PHONES still has the placeholder +1234567890 - SMS won't reach anyone"));
    }
  }
#if THINGSPEAK_ENABLED
  if (strncmp_P(API_KEY_NODE_2, PSTR("REPLACE"), 7) == 0) Serial.println(F("WARNING: API_KEY_NODE_2 is a placeholder - node 2 uploads will fail"));
  if (strncmp_P(API_KEY_NODE_3, PSTR("REPLACE"), 7) == 0) Serial.println(F("WARNING: API_KEY_NODE_3 is a placeholder - node 3 uploads will fail"));
#endif
}

// =====================================================================
//  SETUP / LOOP
// =====================================================================

void setup() {
  Serial.begin(9600);
  hc12.begin(9600);
  sim7600.begin(9600);
  Serial.println(F("Base Module Starting..."));

  Serial.println(F("SIM module: connecting (up to 15 s while it boots)..."));
  modemAlive = modemConnect(12);
  if (modemAlive) modemConfigure();

#if !USE_GNSS_QUERY
  strcpy(latLong, SITE_LAT_LONG);
#endif

  Serial.println(F("==== FMS BASE STATION (packet v2) ===="));
  Serial.println(F("node OFFLINE after 10 s (if it sends every ~2 s) or 10 min (every ~10 s)"));
#if FORCE_RELAY_FROM_NODE != 0
  Serial.print(F("RELAY TEST: ignoring direct (hops 0) packets from NODE "));
  Serial.println(FORCE_RELAY_FROM_NODE);
#endif
  Serial.print(F("flood SMS max 1/hour | low-battery SMS "));
  Serial.print(LOW_BATT_SMS ? F("ON") : F("OFF"));
  Serial.print(F(" | STATUS texts "));
  Serial.println(STATUS_SMS_ENABLED ? F("ON") : F("OFF"));
#if ALERT_RULES_BRIEF
  Serial.println(F("alert rule: water > 30 or soil <= 436, held 3 readings; clears at soil > 470 and water <= 30"));
#else
  Serial.println(F("alert rule: soil < 450 or water > 200 (single reading)"));
#endif
#if RISK_LEVELS
  Serial.println(F("risk levels ON (display only): NORMAL / WATCH / WARNING / FLOOD, ThingSpeak field8"));
#endif
  Serial.println(THINGSPEAK_ENABLED ? F("ThingSpeak uploads ON") : F("ThingSpeak uploads OFF (THINGSPEAK_ENABLED 0)"));
  Serial.print(F("map location: "));
  Serial.println(USE_GNSS_QUERY ? F("GNSS (0.0,0.0 until fix)") : F("provisioned SITE_LAT_LONG"));
  printModemStatus();
  printStartupWarnings();
  printHelp();
  Serial.println(F("Listening for nodes..."));

  wdt_enable(WDTO_8S);
  hc12.listen();
  lastStatusPoll = millis();
}

void loop() {
  wdt_reset();
  hc12.listen();          // no-op unless a modem function switched away

  pollRadio();
  handleSerialCommands();
  processNodes();
  checkOfflineNodes();

  if (!modemAlive && millis() - lastModemCheck >= MODEM_RECHECK_MS && radioQuiet()) {
    lastModemCheck = millis();
    if (modemConnect(1)) {
      modemAlive = true;
      Serial.println(F("SIM module answering again"));
      modemConfigure();
    }
    hc12.listen();
  }

#if STATUS_SMS_ENABLED
  if (modemAlive && millis() - lastStatusPoll >= STATUS_POLL_MS && !alertPendingAny() &&
      (radioQuiet() || millis() - lastStatusPoll >= STATUS_POLL_MS + 10000UL)) {
    lastStatusPoll = millis();
    pollStatusSms();
  }
#endif
}
