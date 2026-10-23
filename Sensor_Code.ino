/* =====================================================================
   FMS - Flood Monitoring System
   SENSOR / RELAY NODE  (Sensor_Code.ino)      firmware 2026-09-26, packet v2
   Board: Arduino Nano (ATmega328P, 5 V, 16 MHz)

   !! The packet format changed. Reflash the base AND every transmitter. !!

   WIRING (every pin this sketch touches)
   ---------------------------------------------------------------------
   D0, D1  USB serial (Serial Monitor, 9600 baud). Nothing else on these.
   D8      <- HC-12 TXD   (SoftwareSerial RX)
   D10     -> HC-12 RXD   (SoftwareSerial TX)
   D2, D3  not used
   D4      -> soil + water sensor VCC (power-gated, HIGH only while reading)
   D5      -> rain sensor module VCC (power-gated). Rain is read while D4 is
              also on, so a rain module powered from D4 or 5V works too.  NEW
   D6, D7  soil probe drive pins - ONLY when SOIL_AC_DRIVE is 1 (see below)
   D9      HC-12 SET: NOT used. Leave it unconnected (the HC-12 pulls SET
           high internally = normal mode, factory settings untouched).
   D13     onboard LED, blinks on every send and every relay          NEW
   A0      <- soil moisture sensor AO
            With SOIL_AC_DRIVE 1 the soil module is removed and the fork is
            wired straight to the Nano:
               D6 --[10k]--+-- probe prong 1        D7 ------ probe prong 2
                           |
                           A0
   A1      <- water level sensor AO
   A2      <- rain sensor module AO (FC-37 / YL-83; DO pin unused)    NEW
   A3      free
   A4      <-> INA219 SDA (I2C)                                       NEW
   A5      <-> INA219 SCL (I2C)                                       NEW
   A6      free
   A7      leave UNCONNECTED: the floating pin seeds random() at boot NEW
   5V      from the MT3608 boost output: HC-12 VCC, INA219 VCC
   GND     common ground for everything

   INA219 (address 0x40, 0.1 ohm shunt) in the battery positive line:
     LiPo +  ->  INA219 VIN+          INA219 VIN-  ->  charge module BAT+
   Bus + shunt voltage = battery voltage. Current = net battery current.

   PACKET (every send interval; other nodes relay it):
     <origin,seq,hops,soil,water,rain,batt_pct,batt_mv,avg_ma,flags*CK>
     e.g. <1,137,0,612,45,410,87,4080,120,6*1F>  (max 49 chars + CR LF)
     CK = XOR of every char between '<' and '*', as 2 hex digits.
     rain/batt_pct/batt_mv/avg_ma = -1 when that sensor is missing.
     avg_ma = 10-minute average battery current, + charging / - discharging.
   ===================================================================== */

#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/power.h>

// ======================= SET THIS FOR EACH BOARD =======================
#define NODE_ID 1   // unique per transmitter, 1..254. CHANGE BEFORE FLASHING EACH BOARD.

// ============== FEATURE SWITCHES (1 = on, 0 = off if it breaks) ==============
#define DEMO_MODE        0  // 1: send every 2 s (filming). 0: normal 10 s interval (saves battery).
#define RELAY_ENABLED    1  // 1: rebroadcast other nodes' packets (multi-hop).
#define BATTERY_MONITOR  1  // 1: read the INA219. 0: skip it, battery fields sent as -1.
#define RAIN_SENSOR      1  // 1: read the rain sensor on A2. 0: skip it, rain sent as -1.
#define TX_LED_BLINK     1  // 1: blink the D13 LED on every send and relay.
#define SERIAL_COMMANDS  1  // 1: single-key commands in the Serial Monitor (h = help).
#define SEND_JITTER      1  // 1: add 0-10% random jitter to the send interval so two
                            //    nodes can't lock into colliding every single time.
#define BATTERY_SAVER    1  // 1: when the battery is CRITICAL and not charging, send every 60 s instead of
                            //    every 2/10 s (still relays for other nodes). Leaves saver above 15% or
                            //    as soon as it charges. Saves ~2% in normal mode, ~10% in demo mode.
// ---------------- POWER SAVING ----------------
#define POWER_MODE       2  // 0: always awake, relays for other nodes (the old behaviour).
                            // 1: always sleep between readings - end node, does NOT relay.
                            // 2: AUTO - relay while the battery is healthy or charging,
                            //    sleep between readings when it runs low. Readings and alerts
                            //    are sent exactly the same way in every mode.
#define SLEEP_BELOW_PCT  50 // AUTO: start sleeping at or below this % while not charging   // UNCALIBRATED
#define RELAY_ABOVE_PCT  60 // AUTO: back to relaying at or above this %, or when charging  // UNCALIBRATED
#define SLEEP_WITHOUT_BATT_DATA 1 // AUTO with no INA219 reading: 1 = sleep, 0 = stay awake and relay
#define HC12_SET_PIN     9  // HC-12 SET pin. Wire HC-12 SET -> D9 so the radio itself can sleep
                            // (~16 mA -> ~0.02 mA). Without the wire only the Nano sleeps.
#define CPU_IDLE_NAP     1  // 1: while awake, the CPU naps between tasks (radio still hears everything)
#define SOIL_AC_DRIVE    0  // 1: soil fork wired to D6/D7 + 10k (no module). Current flips direction
                            //    on every sample so the prongs don't corrode. Recalibrate after
                            //    switching. 0: soil module on A0, powered from D4 (as before).

#if BATTERY_MONITOR
#include <Wire.h>
#include <Adafruit_INA219.h>
#endif

// --- PINS & HARDWARE ---
SoftwareSerial hc12(8, 10);        // RX = D8 (from HC-12 TXD), TX = D10 (to HC-12 RXD)
const int MOISTURE_PIN = A0;
const int WATER_LEVEL_PIN = A1;
const int SENSOR_POWER_PIN = 4;
const int SOIL_DRIVE_A_PIN = 6;    // NEW, SOIL_AC_DRIVE only: through 10k to prong 1 (and A0)
const int SOIL_DRIVE_B_PIN = 7;    // NEW, SOIL_AC_DRIVE only: straight to prong 2
#define SOIL_AC_SETTLE_MS 10       // time in each direction per sample (the calibration logger used 10 ms)
const int RAIN_PIN = A2;           // NEW
const int RAIN_POWER_PIN = 5;      // NEW
const int TX_LED_PIN = 13;         // NEW, onboard LED
const int RANDOM_SEED_PIN = A7;    // NEW, must stay unconnected

// --- TIMING ---
const unsigned long READ_INTERVAL_MS = 10000;  // normal send interval (unchanged)
const unsigned long DEMO_INTERVAL_MS = 2000;   // send interval when DEMO_MODE is 1

// --- MULTI-HOP RELAY ---
#define MAX_HOPS              3        // a packet is relayed at most 3 times
#define RELAY_BACKOFF_MIN_MS  50       // random wait before relaying (half-duplex radio)
#define RELAY_BACKOFF_MAX_MS  300
#define DUP_CACHE_SIZE        16       // remembered (origin, seq) pairs
#define DUP_EXPIRY_MS         30000UL  // forget a pair after 30 s (so a rebooted node isn't ignored)
#define RELAY_QUEUE_SIZE      3        // relays waiting for their backoff
#define CHANNEL_QUIET_MS      20       // listen-before-talk: radio must be silent this long
#define MAX_CHANNEL_WAIT_MS   1000     // ...but never wait longer than this to transmit

// --- BATTERY (INA219) ---
#define BATTERY_CAPACITY_MAH  2200
#define CURRENT_SIGN          (-1)     // -1 for your wiring (VIN+ on the LiPo) so CHARGING reads +.
                                       // If CHARGING/DISCHARGING come out swapped, set to 1.
#define CHARGE_DEADBAND_MA    5        // |current| <= 5 mA = IDLE
#define BATT_LOW_PCT          20       // UNCALIBRATED
#define BATT_CRITICAL_PCT     10       // UNCALIBRATED
#define BATT_SAMPLE_MS        1000UL   // one INA219 reading per second
#define BATT_MIN_VALID_MV     2500     // outside 2.5-4.5 V = bad reading, battery sent as -1
#define BATT_MAX_VALID_MV     4500
#define INA_RETRY_MS          30000UL  // if the INA219 is missing, look for it again every 30 s
#define SAVER_INTERVAL_MS     60000UL  // send interval while BATTERY_SAVER is active
#define SAVER_EXIT_PCT        (BATT_CRITICAL_PCT + 5)   // gap so it doesn't flick on/off at 10%

// --- RAIN SENSOR (FC-37 / YL-83 analog output) ---
#define RAIN_WET_IS_LOW   1    // 1: reads LOWER when wet - confirmed for your probe (Sensor Findings Brief,
                               // 17 Sep: idle ~1023, falls when wet). 0: module reads higher when wet.
                               // Thresholds below are written for the wet-is-low direction.
#define RAIN_WET_BELOW    980  // reading below this = raining. From the Findings Brief: idle ~1023 plus a
                               // 43-count buffer (a chosen allowance, not a measured rain value)
#define RAIN_HEAVY_BELOW  500  // UNCALIBRATED: reading at/below this = HEAVY, between = LIGHT
                               // (the brief says light vs heavy spray hasn't been logged yet)
#define RAIN_FAULT_BELOW  10   // UNCALIBRATED: reading below this = shorted/faulty, sent as -1

// --- SERIAL LABELS ONLY. The BASE decides alerts; keep these equal to Base_Code.ino ---
// These match the base's ALERT_RULES_BRIEF 1 (soil <= 436, water > 30).
// If you set ALERT_RULES_BRIEF 0 on the base, change them to 450 and 200.
#define SOIL_LABEL_ALERT_BELOW   437   // soil below 437 (= 436 or lower) shows SATURATED
#define WATER_LABEL_ALERT_ABOVE  30    // water above 30 shows HIGH
#define SOIL_LABEL_NO_PROBE_FROM 960   // Findings Brief: 960+ = probe out of soil or unplugged

// --- FLAGS BYTE (last packet field) ---
#define FLAG_RAIN_MASK      0x03  // bits 0-1: 0 NONE, 1 LIGHT, 2 HEAVY, 3 no reading
#define FLAG_CHARGING       0x04
#define FLAG_DISCHARGING    0x08  // neither charging nor discharging = IDLE
#define FLAG_BATT_LOW       0x10
#define FLAG_BATT_CRITICAL  0x20
#define FLAG_NO_BATT_DATA   0x40  // INA219 missing or reading out of range
#define FLAG_BOOT           0x80  // one of the first 3 packets after power-up

#define RAIN_NONE   0
#define RAIN_LIGHT  1
#define RAIN_HEAVY  2
#define RAIN_NODATA 3

#define PKT_BUF_SIZE 56

struct Packet {
  uint8_t origin, seq, hops, flags;
  int16_t soil, water, rain, battPct, battMv, avgMa;
};

struct SeenEntry {
  uint8_t origin, seq;        // origin 0 = empty slot
  unsigned long t;
};

struct RelaySlot {
  Packet p;                   // hops already incremented
  unsigned long due;
  bool used;
};

// Serial views
#define VIEW_ALL   0
#define VIEW_SOIL  1
#define VIEW_WATER 2
#define VIEW_RAIN  3
#define VIEW_BATT  4

// LiPo open-circuit voltage -> percent. Interpolated linearly between rows.
const uint16_t LIPO_MV[] PROGMEM = {
  4200, 4150, 4110, 4080, 4020, 3980, 3950, 3910, 3870, 3850,
  3840, 3820, 3800, 3790, 3750, 3730, 3710, 3690, 3610, 3270 };
const uint8_t LIPO_PCT[] PROGMEM = {
  100, 95, 90, 85, 80, 75, 70, 65, 60, 55,
  50, 45, 40, 35, 25, 20, 15, 10, 5, 0 };
const uint8_t LIPO_ROWS = sizeof(LIPO_PCT);

unsigned long lastReadTime = 0;
unsigned long sendInterval = DEMO_MODE ? DEMO_INTERVAL_MS : READ_INTERVAL_MS;
bool sendNow = false;
uint8_t txSeq = 0;
uint8_t bootPacketsLeft = 3;
uint8_t viewMode = VIEW_ALL;

// Own packet waiting for a quiet channel
Packet ownPkt;
bool ownPending = false;

// Last few seqs we sent, to spot another board with the same NODE_ID
uint8_t recentSeqs[4];
uint8_t recentSeqCount = 0, recentSeqIdx = 0;
unsigned long lastIdWarning = 0;

// --- RADIO RX ---
char rxBuf[64];
uint8_t rxLen = 0;
bool rxActive = false;
unsigned long lastRxByteMs = 0;
unsigned long blockedSince = 0;

SeenEntry seen[DUP_CACHE_SIZE];
uint8_t seenNext = 0;
RelaySlot relayQ[RELAY_QUEUE_SIZE];

// Counters for the 'i' command
unsigned int statSent = 0, statRelayed = 0, statDup = 0, statBad = 0, statQueueFull = 0;

// --- LED ---
bool ledIsOn = false;
unsigned long ledOnAt = 0;

// --- BATTERY STATE ---
#if BATTERY_MONITOR
Adafruit_INA219 ina219;   // address 0x40
#endif
bool inaFound = false;
bool battRangeWarned = false;
unsigned long lastBattSample = 0, lastInaRetry = 0, lastGoodBattMs = 0;
int16_t mvRing[10], maRing[10];           // last 10 readings, for display
uint8_t ringCount = 0, ringIdx = 0;
long minuteSumMa = 0;                     // current minute being accumulated
uint16_t minuteSamples = 0;
unsigned long minuteStart = 0;
int16_t minuteAvgMa[10];                  // last 10 one-minute averages = 10-min average
uint8_t minuteCount = 0, minuteIdx = 0;

bool rainFaultWarned = false;
bool saverActive = false;
bool radioSleepOK = false;   // HC-12 SET wired to D9 and answering
bool endNodeMode = false;    // currently sleeping between readings (not relaying)

// --- WATCHDOG SAFETY (runs before setup) ---
// After a watchdog reset the chip keeps the watchdog ON with a 16 ms timeout. A Nano flashed
// with "Upload Using Programmer" has no bootloader to turn it off, so without this it would
// reset forever. Also saves why the board reset, for the boot message.
uint8_t resetFlags __attribute__((section(".noinit")));
void earlyWatchdogOff(void) __attribute__((naked, used, section(".init3")));
void earlyWatchdogOff(void) {
  resetFlags = MCUSR;
  MCUSR = 0;
  wdt_disable();
}

// =====================================================================
//  PACKET FORMAT (identical copy lives in Base_Code.ino - keep in sync)
// =====================================================================

// Writes "<origin,...,flags*CK>" into out. Returns the length, 0 on error.
uint8_t buildPacket(const Packet &p, char *out, uint8_t size) {
  out[0] = '<';
  int n = snprintf_P(out + 1, size - 1, PSTR("%u,%u,%u,%d,%d,%d,%d,%d,%d,%u"),
                     p.origin, p.seq, p.hops, p.soil, p.water, p.rain,
                     p.battPct, p.battMv, p.avgMa, p.flags);
  if (n <= 0 || n + 6 > size) { out[0] = '\0'; return 0; }
  uint8_t ck = 0;
  for (int i = 1; i <= n; i++) ck ^= (uint8_t)out[i];
  snprintf_P(out + 1 + n, size - 1 - n, PSTR("*%02X>"), ck);
  return n + 5;
}

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
//  SMALL PRINT HELPERS (camera-friendly, no String)
// =====================================================================

void printPadded(long v, uint8_t width) {
  long a = v < 0 ? -v : v;
  uint8_t digits = 1;
  while (a >= 10) { a /= 10; digits++; }
  if (v < 0) digits++;
  while (digits++ < width) Serial.print(' ');
  Serial.print(v);
}

void printSigned(int v) {
  if (v >= 0) Serial.print('+');
  Serial.print(v);
}

void printVolts(int mv) {
  int cv = (mv + 5) / 10;          // centivolts, rounded
  Serial.print(cv / 100);
  Serial.print('.');
  if (cv % 100 < 10) Serial.print('0');
  Serial.print(cv % 100);
  Serial.print('V');
}

void printHours(long tenths) {
  Serial.print(tenths / 10);
  Serial.print('.');
  Serial.print(tenths % 10);
  Serial.print('h');
}

int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

// =====================================================================
//  RADIO
// =====================================================================

void rememberOwnSeq(uint8_t s) {
  recentSeqs[recentSeqIdx] = s;
  recentSeqIdx = (recentSeqIdx + 1) % 4;
  if (recentSeqCount < 4) recentSeqCount++;
}

bool sentRecently(uint8_t s) {
  for (uint8_t i = 0; i < recentSeqCount; i++) if (recentSeqs[i] == s) return true;
  return false;
}

void handleIncoming(char *body) {
  Packet p;
  if (!parsePacket(body, p)) { statBad++; return; }

  if (p.origin == NODE_ID) {
    // Our own packet echoed back by a relay: never rebroadcast it.
    // A seq we never sent means a second board has the same NODE_ID.
    if (!sentRecently(p.seq) && (lastIdWarning == 0 || millis() - lastIdWarning > 30000UL)) {
      lastIdWarning = millis();
      Serial.print(F("WARNING: another board is also NODE_ID "));
      Serial.print(NODE_ID);
      Serial.println(F(" - give every board a unique NODE_ID!"));
    }
    return;
  }

  if (seenBefore(p.origin, p.seq)) { statDup++; return; }

#if RELAY_ENABLED
  if (p.hops < MAX_HOPS) {
    for (uint8_t i = 0; i < RELAY_QUEUE_SIZE; i++) {
      if (!relayQ[i].used) {
        relayQ[i].p = p;
        relayQ[i].p.hops = p.hops + 1;
        relayQ[i].due = millis() + random(RELAY_BACKOFF_MIN_MS, RELAY_BACKOFF_MAX_MS + 1);
        relayQ[i].used = true;
        return;
      }
    }
    statQueueFull++;
  }
#endif
}

// Moves bytes from the HC-12 into rxBuf. A complete <...> is handled immediately.
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
    if (c == '\r' || c == '\n' || rxLen >= sizeof(rxBuf) - 1) {  // broken packet
      rxActive = false;
      statBad++;
      continue;
    }
    rxBuf[rxLen++] = c;
  }
  if (rxActive && millis() - lastRxByteMs > 100) rxActive = false;  // packet cut off mid-air
}

// Like delay(), but keeps draining the radio so nothing is lost while we wait.
void serviceDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) pollRadio();
}

// Serial at 9600 baud is slow. Before printing a line, wait for room in the
// USB TX buffer while still draining the radio, so printing never blocks it.
void serialRoom() {
  while (Serial.availableForWrite() < SERIAL_TX_BUFFER_SIZE - 2) pollRadio();
}

bool channelBusy() {
  return rxActive || (millis() - lastRxByteMs < CHANNEL_QUIET_MS);
}

void ledOn() {
#if TX_LED_BLINK
  digitalWrite(TX_LED_PIN, HIGH);
  ledIsOn = true;
  ledOnAt = millis();
#endif
}

void serviceLed() {
  if (ledIsOn && millis() - ledOnAt >= 60) {
    digitalWrite(TX_LED_PIN, LOW);
    ledIsOn = false;
  }
}

void transmitPacket(const Packet &p, char *out) {
  if (!buildPacket(p, out, PKT_BUF_SIZE)) return;
  ledOn();
  hc12.println(out);
}

// Sends at most one packet per call: our own first, then any relay whose backoff is over.
void serviceTransmit() {
  bool relayDue = false;
  for (uint8_t i = 0; i < RELAY_QUEUE_SIZE; i++) {
    if (relayQ[i].used && (long)(millis() - relayQ[i].due) >= 0) relayDue = true;
  }
  if (!ownPending && !relayDue) { blockedSince = 0; return; }

  if (channelBusy()) {
    if (blockedSince == 0) blockedSince = millis() | 1;
    if (millis() - blockedSince < MAX_CHANNEL_WAIT_MS) return;
  }
  blockedSince = 0;

  char out[PKT_BUF_SIZE];
  if (ownPending) {
    ownPending = false;
    transmitPacket(ownPkt, out);
    statSent++;
    if (viewMode == VIEW_ALL) {
      serialRoom();
      Serial.print(F("TX     "));
      Serial.println(out);
    }
    return;
  }
  for (uint8_t i = 0; i < RELAY_QUEUE_SIZE; i++) {
    if (relayQ[i].used && (long)(millis() - relayQ[i].due) >= 0) {
      Packet rp = relayQ[i].p;      // copy: the slot can be reused while we print
      relayQ[i].used = false;
      transmitPacket(rp, out);
      statRelayed++;
      if (viewMode == VIEW_ALL) {
        serialRoom();
        Serial.print(F("RELAY  node "));
        Serial.print(rp.origin);
        Serial.print(F(" #"));
        Serial.print(rp.seq);
        Serial.print(F("  hop "));
        Serial.println(rp.hops);
      }
      return;
    }
  }
}

// =====================================================================
//  SENSORS
// =====================================================================

// Unchanged: average of 5 readings, 10 ms apart. The pauses now keep listening to the radio.
int getStableReading(int pin) {
  long total = 0;
  for(int i = 0; i < 5; i++) {
    total += analogRead(pin);
    serviceDelay(10);
  }
  return total / 5; // Average of 5 readings
}

#if SOIL_AC_DRIVE
// Soil probe driven in both directions. Forward: D6 high, D7 low - A0 reads the same
// divider the module had (lower = wetter). Reverse: D7 high, D6 low for the same time,
// with a dummy read so both halves last equally long. Net current through the soil is
// about zero, so the prongs don't plate or corrode. Average of 5 forward reads, as before.
int readSoilAC() {
  long total = 0;
  for (uint8_t i = 0; i < 5; i++) {
    digitalWrite(SOIL_DRIVE_B_PIN, LOW);
    digitalWrite(SOIL_DRIVE_A_PIN, HIGH);          // forward
    serviceDelay(SOIL_AC_SETTLE_MS);
    total += analogRead(MOISTURE_PIN);
    digitalWrite(SOIL_DRIVE_A_PIN, LOW);
    digitalWrite(SOIL_DRIVE_B_PIN, HIGH);          // reverse, same length
    serviceDelay(SOIL_AC_SETTLE_MS);
    analogRead(MOISTURE_PIN);
    digitalWrite(SOIL_DRIVE_B_PIN, LOW);           // off
  }
  return total / 5;
}
#endif

// Median of 5, sensor powered only while reading. Returns raw 0-1023, or -1 if off/faulty.
int readRain() {
#if RAIN_SENSOR
  digitalWrite(RAIN_POWER_PIN, HIGH);
  serviceDelay(50);
  int s[5];
  for (uint8_t i = 0; i < 5; i++) {
    s[i] = analogRead(RAIN_PIN);
    serviceDelay(10);
  }
  digitalWrite(RAIN_POWER_PIN, LOW);     // off again: these plates corrode when powered
  for (uint8_t i = 1; i < 5; i++) {      // insertion sort
    int v = s[i];
    int8_t j = i - 1;
    while (j >= 0 && s[j] > v) { s[j + 1] = s[j]; j--; }
    s[j + 1] = v;
  }
  int raw = s[2];
  int reading = RAIN_WET_IS_LOW ? raw : 1023 - raw;
  if (reading < RAIN_FAULT_BELOW) {
    if (!rainFaultWarned) {
      Serial.println(F("WARNING: rain sensor reading out of range - sending -1, alerts unaffected"));
      rainFaultWarned = true;
    }
    return -1;
  }
  rainFaultWarned = false;
  return raw;
#else
  return -1;
#endif
}

uint8_t classifyRain(int raw) {
  if (raw < 0) return RAIN_NODATA;
  int reading = RAIN_WET_IS_LOW ? raw : 1023 - raw;
  if (reading >= RAIN_WET_BELOW) return RAIN_NONE;
  if (reading <= RAIN_HEAVY_BELOW) return RAIN_HEAVY;
  return RAIN_LIGHT;
}

// =====================================================================
//  BATTERY
// =====================================================================

bool startIna() {
#if BATTERY_MONITOR
  Wire.begin();
  Wire.setWireTimeout(3000, true);   // set BEFORE talking to the INA219: a bad I2C wire can't freeze anything
  return ina219.begin();
#else
  return false;
#endif
}

int lipoPercent(int mv) {
  if (mv >= (int)pgm_read_word(&LIPO_MV[0])) return 100;
  for (uint8_t i = 1; i < LIPO_ROWS; i++) {
    int vLo = pgm_read_word(&LIPO_MV[i]);
    if (mv >= vLo) {
      int vHi = pgm_read_word(&LIPO_MV[i - 1]);
      int pHi = pgm_read_byte(&LIPO_PCT[i - 1]);
      int pLo = pgm_read_byte(&LIPO_PCT[i]);
      return pLo + ((long)(mv - vLo) * (pHi - pLo) * 2 + (vHi - vLo)) / (2L * (vHi - vLo));
    }
  }
  return 0;
}

int ringAverage(const int16_t *r) {
  if (ringCount == 0) return 0;
  long sum = 0;
  for (uint8_t i = 0; i < ringCount; i++) sum += r[i];
  return (int)(sum / ringCount);
}

bool battValid() {
  // Sleeping nodes only sample once per cycle, so allow up to 3 cycles between good readings.
  return inaFound && ringCount > 0 && millis() - lastGoodBattMs < 5000UL + 3UL * sendInterval;
}

int displayMv() { return ringAverage(mvRing); }
int displayMa() { return ringAverage(maRing); }

// 10-minute rolling average current: mean of the last 10 one-minute averages.
int avg10MinMa() {
  if (minuteCount == 0) return minuteSamples ? (int)(minuteSumMa / (long)minuteSamples) : displayMa();
  long sum = 0;
  for (uint8_t i = 0; i < minuteCount; i++) sum += minuteAvgMa[i];
  return (int)(sum / minuteCount);
}

uint8_t battStateFlag() {
  int ma = displayMa();
  if (ma > CHARGE_DEADBAND_MA) return FLAG_CHARGING;
  if (ma < -CHARGE_DEADBAND_MA) return FLAG_DISCHARGING;
  return 0;
}

// Tenths of an hour left, or -1 if not discharging.
long runtimeTenths(int pct) {
  int avg = avg10MinMa();
  if (battStateFlag() != FLAG_DISCHARGING || avg >= -CHARGE_DEADBAND_MA) return -1;
  return (long)pct * BATTERY_CAPACITY_MAH / (10L * -avg);
}

void sampleBattery() {
#if BATTERY_MONITOR
  if (!inaFound) {
    if (millis() - lastInaRetry >= INA_RETRY_MS) {
      lastInaRetry = millis();
      if (startIna()) {
        inaFound = true;
        Serial.println(F("INA219 found - battery monitoring ON"));
      }
    }
    return;
  }
  float busV = ina219.getBusVoltage_V();
  bool ok = ina219.success();
  float shuntMv = ina219.getShuntVoltage_mV();
  ok = ok && ina219.success();
  float currentMa = ina219.getCurrent_mA();
  ok = ok && ina219.success();
  long mv = (long)(busV * 1000.0f + shuntMv + 0.5f);
  if (!ok || mv < BATT_MIN_VALID_MV || mv > BATT_MAX_VALID_MV) {
    if (!battRangeWarned) {
      Serial.println(F("WARNING: INA219 reading bad/out of range - battery sent as -1"));
      battRangeWarned = true;
    }
    return;
  }
  battRangeWarned = false;
  float signedMa = CURRENT_SIGN * currentMa;
  int16_t ma = (int16_t)(signedMa >= 0 ? signedMa + 0.5f : signedMa - 0.5f);

  mvRing[ringIdx] = (int16_t)mv;
  maRing[ringIdx] = ma;
  ringIdx = (ringIdx + 1) % 10;
  if (ringCount < 10) ringCount++;
  lastGoodBattMs = millis();

  minuteSumMa += ma;
  minuteSamples++;
  if (millis() - minuteStart >= 60000UL) {
    minuteAvgMa[minuteIdx] = (int16_t)(minuteSumMa / (long)minuteSamples);
    minuteIdx = (minuteIdx + 1) % 10;
    if (minuteCount < 10) minuteCount++;
    minuteSumMa = 0;
    minuteSamples = 0;
    minuteStart = millis();
  }
#endif
}

void fillBatteryFields(Packet &p) {
  if (!battValid()) {
    p.battPct = -1; p.battMv = -1; p.avgMa = -1;
    p.flags |= FLAG_NO_BATT_DATA;
    return;
  }
  int mv = displayMv();
  int pct = lipoPercent(mv);
  p.battPct = pct;
  p.battMv = mv;
  p.avgMa = avg10MinMa();
  p.flags |= battStateFlag();
  if (pct <= BATT_CRITICAL_PCT) p.flags |= FLAG_BATT_CRITICAL | FLAG_BATT_LOW;
  else if (pct <= BATT_LOW_PCT) p.flags |= FLAG_BATT_LOW;
}

// =====================================================================
//  SERIAL OUTPUT
// =====================================================================

void printSoilLine(int soil) {
  serialRoom();
  Serial.print(F("SOIL  "));
  printPadded(soil, 5);
  if (soil >= SOIL_LABEL_NO_PROBE_FROM) Serial.println(F("  no probe? (960+)"));
  else Serial.println(soil < SOIL_LABEL_ALERT_BELOW ? F("  SATURATED") : F("  normal"));
}

void printWaterLine(int water) {
  serialRoom();
  Serial.print(F("WATER "));
  printPadded(water, 5);
  Serial.println(water > WATER_LABEL_ALERT_ABOVE ? F("  HIGH") : F("  normal"));
}

void printRainLine(int raw, uint8_t cls) {
  serialRoom();
  Serial.print(F("RAIN  "));
#if !RAIN_SENSOR
  (void)raw; (void)cls;
  Serial.println(F("   --  off (RAIN_SENSOR 0)"));
#else
  if (cls == RAIN_NODATA) { Serial.println(F("   --  sensor fault")); return; }
  printPadded(raw, 5);
  if (cls == RAIN_NONE) Serial.println(F("  NONE"));
  else if (cls == RAIN_LIGHT) Serial.println(F("  LIGHT"));
  else Serial.println(F("  HEAVY"));
#endif
}

void printBattLine() {
  serialRoom();
  Serial.print(F("BATT  "));
  if (!battValid()) {
#if BATTERY_MONITOR
    Serial.println(inaFound ? F("   --  bad reading") : F("   --  INA219 not found"));
#else
    Serial.println(F("   --  off (BATTERY_MONITOR 0)"));
#endif
    return;
  }
  int mv = displayMv();
  int pct = lipoPercent(mv);
  printPadded(pct, 4);
  Serial.print(F("%  "));
  printVolts(mv);
  uint8_t st = battStateFlag();
  if (st == FLAG_CHARGING) Serial.print(F("  CHARGING "));
  else if (st == FLAG_DISCHARGING) Serial.print(F("  DISCHARGING "));
  else Serial.print(F("  IDLE "));
  printSigned(displayMa());
  Serial.print(F("mA"));
  long h = runtimeTenths(pct);
  if (h >= 0) {
    Serial.print(F("  ~"));
    printHours(h);
    Serial.print(F(" left"));
  }
  if (pct <= BATT_CRITICAL_PCT) Serial.print(F("  CRITICAL"));
  else if (pct <= BATT_LOW_PCT) Serial.print(F("  LOW"));
  if (saverActive) Serial.print(F("  SAVER"));
  Serial.println();
}

void printBatteryNote() {
  Serial.println(F("NOTE: battery % reads high while charging and dips during radio TX."));
  Serial.println(F("      That's expected - the averaging smooths it out."));
}

void printReading(const Packet &p) {
  uint8_t cls = p.flags & FLAG_RAIN_MASK;
  switch (viewMode) {
    case VIEW_SOIL:  printSoilLine(p.soil); break;
    case VIEW_WATER: printWaterLine(p.water); break;
    case VIEW_RAIN:  printRainLine(p.rain, cls); break;
    case VIEW_BATT:  printBattLine(); break;
    default:
      serialRoom();
      Serial.println();
      Serial.print(F("---- NODE "));
      Serial.print(NODE_ID);
      Serial.print(F("  #"));
      Serial.print(p.seq);
      Serial.println(F(" ----"));
      printSoilLine(p.soil);
      printWaterLine(p.water);
      printRainLine(p.rain, cls);
      printBattLine();
      break;
  }
}

void printHelp() {
  Serial.println(F("COMMANDS  s soil | w water | r rain | b battery | a all"));
  Serial.println(F("          i info | t send now | h help"));
}

void printInfo() {
  Serial.print(F("NODE "));
  Serial.print(NODE_ID);
  Serial.print(RELAY_ENABLED ? F(" | relay ON, max ") : F(" | relay OFF, max "));
  Serial.print(MAX_HOPS);
  Serial.print(F(" hops | send every "));
  Serial.print(sendInterval / 1000.0, 1);
  Serial.print(DEMO_MODE ? F(" s (DEMO)") : F(" s"));
  Serial.print(saverActive ? F(" | BATTERY SAVER ON") : F(""));
  Serial.println(endNodeMode ? F(" | SLEEPING (not relaying)") : F(" | AWAKE (relaying)"));
  Serial.print(F("sent "));        Serial.print(statSent);
  Serial.print(F(" | relayed "));  Serial.print(statRelayed);
  Serial.print(F(" | dup "));      Serial.print(statDup);
  Serial.print(F(" | bad rx "));   Serial.print(statBad);
  Serial.print(F(" | relay queue full ")); Serial.println(statQueueFull);
  Serial.print(inaFound ? F("INA219 OK") : F("INA219 MISSING"));
  Serial.print(RAIN_SENSOR ? F(" | rain ON") : F(" | rain OFF"));
  Serial.print(F(" | free RAM "));  Serial.print(freeRam());
  Serial.print(F(" B | up "));      Serial.print(millis() / 1000);
  Serial.println(F(" s"));
}

void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    switch (c) {
      case 'a': viewMode = VIEW_ALL;   Serial.println(F("VIEW  all sensors")); break;
      case 's': viewMode = VIEW_SOIL;  Serial.println(F("VIEW  soil only")); break;
      case 'w': viewMode = VIEW_WATER; Serial.println(F("VIEW  water only")); break;
      case 'r': viewMode = VIEW_RAIN;  Serial.println(F("VIEW  rain only")); break;
      case 'b': viewMode = VIEW_BATT;  Serial.println(F("VIEW  battery only")); printBatteryNote(); break;
      case 'i': printInfo(); break;
      case 't': sendNow = true; break;
      case 'h': case '?': printHelp(); break;
      default: break;   // ignore CR/LF and anything else
    }
  }
}

// =====================================================================
//  SETUP / LOOP
// =====================================================================

// Why the board last reset. Repeated "brown-out" or "watchdog" = power or firmware trouble.
void printResetCause() {
  if (!resetFlags) return;            // a bootloader already cleared it
  Serial.print(F("last reset: "));
  if (resetFlags & _BV(WDRF)) Serial.println(F("WATCHDOG (firmware got stuck)"));
  else if (resetFlags & _BV(BORF)) Serial.println(F("BROWN-OUT (supply voltage dipped - check MT3608/battery)"));
  else if (resetFlags & _BV(EXTRF)) Serial.println(F("reset button / programmer"));
  else Serial.println(F("power on"));
}

// This Nano has no working USB, so the LED is its only local status light.
// Boot: one long blink = firmware running. Then 2 quick blinks = INA219 not found,
// 3 quick blinks = rain sensor gives no reading. After that: a short blink per send/relay.
void blinkTimes(uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    digitalWrite(TX_LED_PIN, HIGH); delay(150);
    digitalWrite(TX_LED_PIN, LOW);  delay(250);
  }
  delay(600);
}

void bootBlink() {
  digitalWrite(TX_LED_PIN, HIGH); delay(1000);
  digitalWrite(TX_LED_PIN, LOW);  delay(600);
#if BATTERY_MONITOR
  if (!inaFound) blinkTimes(2);
#endif
#if RAIN_SENSOR
  digitalWrite(SENSOR_POWER_PIN, HIGH);   // same conditions as a normal reading
  int r = readRain();
  digitalWrite(SENSOR_POWER_PIN, LOW);
  if (r < 0) blinkTimes(3);
#endif
}

// =====================================================================
//  POWER SAVING
//  Awake (relay) mode: radio always listening, CPU naps between tasks.
//  Sleep (end-node) mode, each cycle: wake -> battery reading -> radio on -> read sensors ->
//  send -> radio to sleep (AT+SLEEP via SET) -> whole chip powers down until the next reading.
//  Same readings, same packets, same alerts. Only relaying for other nodes stops while asleep.
// =====================================================================

extern volatile unsigned long timer0_millis;   // Arduino's millis counter (stops while powered down)

volatile bool wdtFired = false;
ISR(WDT_vect) { wdtFired = true; }   // watchdog used only as a wake-up alarm while sleeping

// Puts the HC-12 in command mode (SET low), sends an AT command, waits for `expect`.
bool hc12Command(const __FlashStringHelper *cmd, char expect) {
  digitalWrite(HC12_SET_PIN, LOW);
  delay(60);                                   // HC-12 needs ~40 ms to enter command mode
  while (hc12.available()) hc12.read();
  hc12.print(cmd);
  hc12.print(F("\r\n"));
  bool ok = false;
  unsigned long t = millis();
  while (millis() - t < 250) {
    while (hc12.available()) if (hc12.read() == expect) ok = true;
  }
  return ok;
}

bool hc12Probe() {
  bool ok = hc12Command(F("AT"), 'K');        // reply "OK"
  digitalWrite(HC12_SET_PIN, HIGH);
  delay(100);                                  // back to normal radio mode
  while (hc12.available()) hc12.read();
  return ok;
}

void hc12Sleep() {
  if (!radioSleepOK) return;
  hc12Command(F("AT+SLEEP"), 'K');             // "OK+SLEEP"; sleeps once SET goes high
  digitalWrite(HC12_SET_PIN, HIGH);
  delay(20);
}

void hc12Wake() {
  if (!radioSleepOK) return;
  digitalWrite(HC12_SET_PIN, LOW);             // command mode wakes it
  delay(60);
  digitalWrite(HC12_SET_PIN, HIGH);            // back to normal radio mode
  delay(100);
  while (hc12.available()) hc12.read();
}

// Whole chip off (a few microamps) for `ms`, woken by the watchdog alarm.
void deepSleepMs(unsigned long ms) {
  Serial.flush();                              // let the last line finish printing
  digitalWrite(TX_LED_PIN, LOW);
  ledIsOn = false;
  hc12.stopListening();                        // radio bytes must not wake us (not relaying now)
  uint8_t timsk0 = TIMSK0;
  TIMSK0 = 0;                                  // Arduino's 1 ms tick would wake us instantly
  while (ms >= 16) {
    uint8_t wdto; unsigned long chunk;
    if (ms >= 8000)      { wdto = WDTO_8S;   chunk = 8000; }
    else if (ms >= 4000) { wdto = WDTO_4S;   chunk = 4000; }
    else if (ms >= 2000) { wdto = WDTO_2S;   chunk = 2000; }
    else if (ms >= 1000) { wdto = WDTO_1S;   chunk = 1000; }
    else if (ms >= 500)  { wdto = WDTO_500MS; chunk = 500; }
    else if (ms >= 250)  { wdto = WDTO_250MS; chunk = 250; }
    else if (ms >= 120)  { wdto = WDTO_120MS; chunk = 120; }
    else if (ms >= 60)   { wdto = WDTO_60MS;  chunk = 60; }
    else if (ms >= 30)   { wdto = WDTO_30MS;  chunk = 30; }
    else                 { wdto = WDTO_15MS;  chunk = 15; }
    uint8_t bits = (wdto & 7) | ((wdto & 8) ? _BV(WDP3) : 0);
    wdtFired = false;
    do {                                       // only the watchdog alarm counts as "time passed"
      cli();
      wdt_reset();
      MCUSR &= ~_BV(WDRF);
      WDTCSR = _BV(WDCE) | _BV(WDE);
      WDTCSR = _BV(WDIE) | _BV(WDIF) | bits;   // alarm only, no reset, while asleep
      set_sleep_mode(SLEEP_MODE_PWR_DOWN);
      sleep_enable();
      sleep_bod_disable();
      sei();
      sleep_cpu();
      sleep_disable();
    } while (!wdtFired);
    cli();
    timer0_millis += chunk;                    // keep millis() roughly right
    sei();
    ms -= chunk;
  }
  TIFR0 = _BV(TOV0);
  TIMSK0 = timsk0;                             // millis() ticking again
  wdt_enable(WDTO_8S);                         // normal watchdog again while awake
  hc12.listen();
}

// Light nap while awake: wakes on any interrupt (radio byte, USB key, 1 ms timer tick).
void idleNap() {
#if CPU_IDLE_NAP
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  sleep_cpu();
  sleep_disable();
#endif
}

// Decides relay (awake) vs end node (sleep). Called once per reading.
void updatePowerMode() {
  bool want;
#if POWER_MODE == 0
  want = false;
#elif POWER_MODE == 1
  want = true;
#else
  want = endNodeMode;
  if (!battValid()) {
    want = SLEEP_WITHOUT_BATT_DATA;
  } else {
    int pct = lipoPercent(displayMv());
    if (battStateFlag() == FLAG_CHARGING || pct >= RELAY_ABOVE_PCT) want = false;
    else if (pct <= SLEEP_BELOW_PCT) want = true;
  }
#endif
  if (want != endNodeMode) {
    endNodeMode = want;
    Serial.println(want ? F("POWER: sleeping between readings (end node, not relaying)")
                        : F("POWER: awake, relaying for other nodes"));
    if (!want) hc12Wake();                     // make sure the radio is listening again
  }
}

// One full cycle in sleep mode: everything a normal reading does, then power down.
void sleepCycle() {
  unsigned long start = millis();
#if BATTERY_MONITOR
  if (inaFound) { ina219.powerSave(false); delay(3); }
  sampleBattery();
#endif
  hc12Wake();
  updateBatterySaver();
  sendInterval = nextInterval();
  readSensorsAndQueuePacket();
  unsigned long t = millis();
  while (ownPending && millis() - t < 1500) { pollRadio(); serviceTransmit(); }
  serviceDelay(80);                            // let the HC-12 finish sending over the air
  lastReadTime = millis();
  updatePowerMode();
  if (!endNodeMode) return;                    // switched back to relaying: stay awake
#if SERIAL_COMMANDS
  handleSerialCommands();
#endif
  hc12Sleep();
#if BATTERY_MONITOR
  if (inaFound) ina219.powerSave(true);
#endif
  unsigned long used = millis() - start;
  deepSleepMs(sendInterval > used ? sendInterval - used : 0);
  lastReadTime = millis() - sendInterval;      // next loop pass starts the next reading right away
}

void seedRandom() {
  unsigned long seed = 0;
  for (uint8_t i = 0; i < 32; i++) {
    seed = (seed << 3) ^ (seed >> 29) ^ (unsigned long)analogRead(RANDOM_SEED_PIN);
    delayMicroseconds(300);
  }
  seed ^= (unsigned long)NODE_ID * 2654435761UL;   // different nodes never share a sequence
  randomSeed(seed);
}

// Battery saver: on at CRITICAL while not charging; off once charging or above SAVER_EXIT_PCT.
// No battery data = never on (sending normally is the safe choice).
void updateBatterySaver() {
#if BATTERY_SAVER
  bool want = saverActive;
  if (!battValid()) {
    want = false;
  } else {
    int pct = lipoPercent(displayMv());
    if (battStateFlag() == FLAG_CHARGING || pct > SAVER_EXIT_PCT) want = false;
    else if (pct <= BATT_CRITICAL_PCT) want = true;
  }
  if (want != saverActive) {
    saverActive = want;
    Serial.println(want ? F("BATTERY SAVER ON: battery critical, sending every 60 s")
                        : F("BATTERY SAVER OFF: back to the normal send interval"));
  }
#endif
}

unsigned long nextInterval() {
  unsigned long base = DEMO_MODE ? DEMO_INTERVAL_MS : READ_INTERVAL_MS;
  if (saverActive) base = SAVER_INTERVAL_MS;
#if SEND_JITTER
  return base + random(0, base / 10 + 1);
#else
  return base;
#endif
}

void readSensorsAndQueuePacket() {
  // Power ON sensors briefly
  digitalWrite(SENSOR_POWER_PIN, HIGH);
  serviceDelay(50);

#if SOIL_AC_DRIVE
  int soilMoisture = readSoilAC();
#else
  int soilMoisture = getStableReading(MOISTURE_PIN);
#endif
  int waterLevel = getStableReading(WATER_LEVEL_PIN);

  // Rain is read before D4 goes off, so it works whether the rain module's VCC is on
  // D5 (wiring block), shares D4 with soil/water (your Sep 5 sketch), or sits on 5V.
  int rainRaw = readRain();

  // Power OFF sensors to stop corrosion
  digitalWrite(SENSOR_POWER_PIN, LOW);

  Packet p;
  p.origin = NODE_ID;
  p.seq = txSeq++;
  p.hops = 0;
  p.soil = soilMoisture;
  p.water = waterLevel;
  p.rain = rainRaw;
  p.flags = classifyRain(rainRaw);
  fillBatteryFields(p);
  if (bootPacketsLeft) { p.flags |= FLAG_BOOT; bootPacketsLeft--; }

  ownPkt = p;
  ownPending = true;
  rememberOwnSeq(p.seq);
  printReading(p);
}

void setup() {
  Serial.begin(9600);
  hc12.begin(9600);

  pinMode(MOISTURE_PIN, INPUT);
  pinMode(WATER_LEVEL_PIN, INPUT);

  // Setup power control for sensors
  pinMode(SENSOR_POWER_PIN, OUTPUT);
  digitalWrite(SENSOR_POWER_PIN, LOW);
#if SOIL_AC_DRIVE
  pinMode(SOIL_DRIVE_A_PIN, OUTPUT);
  digitalWrite(SOIL_DRIVE_A_PIN, LOW);
  pinMode(SOIL_DRIVE_B_PIN, OUTPUT);
  digitalWrite(SOIL_DRIVE_B_PIN, LOW);
#endif
  pinMode(RAIN_POWER_PIN, OUTPUT);
  digitalWrite(RAIN_POWER_PIN, LOW);
  pinMode(RAIN_PIN, INPUT);
  pinMode(TX_LED_PIN, OUTPUT);
  digitalWrite(TX_LED_PIN, LOW);

  seedRandom();
  txSeq = random(256);

  Serial.println();
  Serial.print(F("==== FMS SENSOR NODE "));
  Serial.print(NODE_ID);
  Serial.println(F(" ===="));
  Serial.print(F("Sensor Node "));
  Serial.print(NODE_ID);
  Serial.println(F(" Starting... (packet v2 - base must run the new firmware too)"));
  Serial.print(RELAY_ENABLED ? F("relay ON (max ") : F("relay OFF (max "));
  Serial.print(MAX_HOPS);
  Serial.print(F(" hops) | "));
  Serial.println(DEMO_MODE ? F("DEMO MODE: sending every 2 s") : F("sending every 10 s"));

#if BATTERY_MONITOR
  inaFound = startIna();
  if (inaFound) {
    sampleBattery();
    Serial.print(F("INA219 found at 0x40 - battery "));
    if (battValid()) {
      printVolts(displayMv());
      Serial.print(' ');
      Serial.print(lipoPercent(displayMv()));
      Serial.println('%');
    } else {
      Serial.println(F("reading out of range (check VIN+/VIN- wiring)"));
    }
    printBatteryNote();
  } else {
    Serial.println(F("WARNING: INA219 NOT FOUND at 0x40 (check A4/A5 wiring)."));
    Serial.println(F("         Battery fields will be sent as -1. Everything else keeps running."));
  }
  minuteStart = millis();
  lastBattSample = millis();
  lastInaRetry = millis();
#else
  Serial.println(F("battery monitor OFF (BATTERY_MONITOR 0)"));
#endif
  printHelp();
  printResetCause();

  // Unused on-chip parts off (saves a little, changes nothing we use)
  power_spi_disable();
  power_timer1_disable();
  power_timer2_disable();

  pinMode(HC12_SET_PIN, OUTPUT);
  digitalWrite(HC12_SET_PIN, HIGH);            // normal radio mode
  delay(100);
  radioSleepOK = hc12Probe();
  Serial.println(radioSleepOK ? F("HC-12 SET wired to D9: radio can sleep")
                              : F("HC-12 SET not on D9: only the Nano can sleep (wire SET->D9 for big savings)"));
#if POWER_MODE == 0
  Serial.println(F("POWER MODE: always awake (relay)"));
#elif POWER_MODE == 1
  Serial.println(F("POWER MODE: always sleep between readings (end node)"));
#else
  Serial.println(F("POWER MODE: AUTO - relay when battery healthy, sleep when low"));
#endif
  bootBlink();

  wdt_enable(WDTO_8S);
}

void loop() {
  wdt_reset();

  if (endNodeMode) {                           // sleeping node: one reading per wake-up
    if (millis() - lastReadTime >= sendInterval) sleepCycle();
    return;
  }

  unsigned long currentMillis = millis();

  // --- 1. RADIO: listen, check, and schedule relays ---
  pollRadio();

#if SERIAL_COMMANDS
  handleSerialCommands();
#endif

#if BATTERY_MONITOR
  if (currentMillis - lastBattSample >= BATT_SAMPLE_MS) {
    lastBattSample = currentMillis;
    sampleBattery();
  }
#endif

  // --- 2. NORMAL TRANSMIT: read sensors and queue our own packet ---
  if (sendNow || currentMillis - lastReadTime >= sendInterval) {
    lastReadTime = currentMillis;
    sendNow = false;
    updateBatterySaver();
    sendInterval = nextInterval();
    readSensorsAndQueuePacket();
    updatePowerMode();
    if (endNodeMode) {                         // just switched to sleep mode: finish this send first
      unsigned long t = millis();
      while (ownPending && millis() - t < 1500) { pollRadio(); serviceTransmit(); }
      serviceDelay(80);
      hc12Sleep();
#if BATTERY_MONITOR
      if (inaFound) ina219.powerSave(true);
#endif
      deepSleepMs(sendInterval);
      lastReadTime = millis() - sendInterval;
      return;
    }
  }

  // --- 3. Send whatever is due, once the channel is quiet ---
  serviceTransmit();
  serviceLed();
  idleNap();
}
