#!/usr/bin/env bash
#
# Does the modem actually transmit over cellular?
#
# This separates two failures that have been getting conflated:
#
#   "the modem cannot transmit"        -- power, SIM, registration, data plan
#   "our endpoint is not reachable"    -- the dead quick-tunnel hostname
#
# It answers the first one WITHOUT needing a tunnel, a public server or a
# domain, by pointing the firmware at a plain-HTTP host that is always up.
# That host has no /update path, so it answers 404 -- and a 404 is the proof
# we want: only a real web server that received our request can produce one.
# DNS failures, dead connections and brownouts cannot fake it.
#
#   ./server/check_cellular.sh
#   ./server/check_cellular.sh --port /dev/cu.usbserial-1130 --seconds 180
#
# The sketch's own SERVER_HOST is restored afterwards. The BOARD is left
# running the test firmware -- go_live.sh reflashes it with the real one.

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH="$REPO/Base_Code.ino"
TEST_HOST="httpforever.com"
FQBN="arduino:avr:nano:cpu=atmega328old"
PORT=""
SECONDS_WATCH=150

while [ $# -gt 0 ]; do
  case "$1" in
    --port)    PORT="$2"; shift 2 ;;
    --seconds) SECONDS_WATCH="$2"; shift 2 ;;
    --host)    TEST_HOST="$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

say(){ printf '\n\033[1m==> %s\033[0m\n' "$1"; }
die(){ printf '\033[31merror: %s\033[0m\n' "$1" >&2; exit 1; }

[ -z "$PORT" ] && PORT=$(ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
[ -n "$PORT" ] || die "no board found. Plug it in, or pass --port /dev/cu.something"

pgrep -f "serial_bridge.py" >/dev/null 2>&1 \
  && die "serial_bridge.py is holding $PORT. Stop it: pkill -f serial_bridge.py"

ORIGINAL=$(grep -oE 'SERVER_HOST\[\] = "[^"]*"' "$SKETCH" | sed 's/.*"\(.*\)"/\1/')
say "board $PORT"
echo "    sketch endpoint : $ORIGINAL"
echo "    test endpoint   : $TEST_HOST  (expected answer: HTTP 404)"

patch_host(){
  python3 - "$SKETCH" "$1" <<'PY'
import re, sys
path, host = sys.argv[1], sys.argv[2]
src = open(path, encoding="utf-8").read()
new, n = re.subn(r'(SERVER_HOST\[\] = ")[^"]*(")', lambda m: m.group(1)+host+m.group(2), src)
if n != 1:
    sys.exit("expected exactly one SERVER_HOST line, found %d" % n)
open(path, "w", encoding="utf-8").write(new)
PY
}
# Restore the real hostname even if the compile, the upload or the capture
# dies: leaving a test host committed into the firmware source would be a
# far worse outcome than a failed diagnostic.
trap 'patch_host "$ORIGINAL"' EXIT

patch_host "$TEST_HOST"

BUILD=$(mktemp -d)/Base_Code
mkdir -p "$BUILD" && cp "$SKETCH" "$BUILD/Base_Code.ino"

say "compile"
arduino-cli compile --fqbn "$FQBN" "$BUILD" | tail -2

say "upload"
if ! arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$BUILD" 2>/tmp/fms_upload.log; then
  echo "    retrying with the newer Nano bootloader"
  arduino-cli compile --fqbn arduino:avr:nano "$BUILD" >/dev/null
  arduino-cli upload -p "$PORT" --fqbn arduino:avr:nano "$BUILD" \
    || { cat /tmp/fms_upload.log >&2; die "upload failed on both bootloaders"; }
fi

LOG=/tmp/fms_cellular_check.log
say "listening for ${SECONDS_WATCH}s  (log: $LOG)"
echo "    the modem needs ~15s to boot and up to a minute to register"
echo

python3 - "$PORT" "$SECONDS_WATCH" "$LOG" <<'PY'
import subprocess, sys, threading, time
port, secs, logpath = sys.argv[1], int(sys.argv[2]), sys.argv[3]
subprocess.run(["stty", "-f", port, "9600", "raw", "-echo"], check=True)

# The base only uploads when a sensor node's packet arrives over the HC-12,
# so waiting passively would test the radio chain as well and confound the
# result. The firmware's 't' command fetches /health over cellular instead:
# no sensor node needed, and nothing is written anywhere.
def drive(fh):
    time.sleep(25)                      # modem boot + network registration
    for cmd in ("m", "t", "t", "t"):
        try:
            fh.write(cmd); fh.flush()
        except Exception:
            return
        time.sleep(30)

end = time.time() + secs
with open(port, "r+", errors="replace", buffering=1) as ser, open(logpath, "w") as out:
    threading.Thread(target=drive, args=(ser,), daemon=True).start()
    while time.time() < end:
        line = ser.readline()
        if not line:
            continue
        line = line.rstrip("\r\n")
        print("   ", line, flush=True)
        out.write(line + "\n"); out.flush()
PY

# --- verdict ---------------------------------------------------------------
say "verdict"
python3 - "$LOG" "$TEST_HOST" <<'PY'
import re, sys
log = open(sys.argv[1], errors="replace").read()
host = sys.argv[2]
L = log.splitlines()

def has(pat): return any(re.search(pat, l) for l in L)

boots = sum(1 for l in L if "Base Module Starting" in l)
# The firmware appends an explanation inside the parens for 6xx codes
# ("FAILED (HTTP 601 - no data connection? ...)"), so do not anchor on ")".
http  = re.findall(r"FAILED \(HTTP (\d+)", log)
ok    = has(r": uploaded")
# 't' command results take priority: they isolate the cellular path from the
# radio chain, so they are the cleaner evidence when both are present.
tested_ok   = has(r"^test: OK")
tested_fail = re.findall(r"test: FAILED \(HTTP (\d+)", log)
tested_dead = has(r"test: FAILED \(") and not tested_fail

def out(tag, colour, msg):
    print("\033[%sm%s\033[0m  %s" % (colour, tag, msg))

if not L:
    out("NO OUTPUT", "31", "Nothing on the serial port at all.\n"
        "   Wrong port, wrong baud (expects 9600), or the board is not running.")
    sys.exit(1)

# Transmit proven: only a real server that received the request can 404.
if tested_ok:
    out("CELLULAR WORKS", "32",
        "The 't' test completed an HTTP request over cellular and got a reply.\n"
        "   Power, SIM, registration, APN and DNS are all fine. Point it at an\n"
        "   endpoint of ours that is reachable: run ./server/go_live.sh")
elif tested_fail:
    code = tested_fail[-1]
    out("NO DATA CONNECTION", "31",
        "HTTP %s is a modem-level error, not a server reply -- the request never\n"
        "   left the modem. The SIM registered, so this is almost always the APN\n"
        "   (currently \"internet\") or an SMS-only plan with no data." % code)
elif tested_dead and boots <= 1:
    out("MODEM STOPPED ANSWERING", "31",
        "The test ran and the modem never replied. If the board did not reset,\n"
        "   suspect the supply sagging far enough to stall the modem without\n"
        "   resetting the Arduino. Measure the modem's VCC during a send.")
elif ok or http:
    code = "200" if ok else http[-1]
    if ok or code == "404":
        out("CELLULAR WORKS", "32",
            "The modem completed an HTTP request over cellular and %s answered "
            "HTTP %s.\n   Power, SIM, registration, APN and DNS are all fine. The "
            "only thing\n   left is pointing it at an endpoint of ours that is "
            "actually reachable:\n   run ./server/go_live.sh" % (host, code))
    elif code.startswith("6"):
        out("NO DATA CONNECTION", "31",
            "HTTP %s is a modem-level error, not a server reply. The request never\n"
            "   left the modem. Check the APN (currently \"internet\") and that the\n"
            "   SIM has a DATA plan -- an SMS-only plan registers fine and still\n"
            "   fails here." % code)
    else:
        out("REACHED A SERVER", "33",
            "HTTP %s came back, so the radio path works. Unexpected code for a\n"
            "   test host -- check what %s is serving." % (code, host))
elif boots > 1:
    out("RESETTING", "31",
        "The board restarted %d times during the test and never finished an\n"
        "   upload. The watchdog is fed correctly in every modem wait loop, so\n"
        "   this is the supply collapsing when the transmitter keys up.\n"
        "   Own 3.7-4.2V supply + 1000uF across the modem's VCC/GND, common\n"
        "   ground with the Arduino. See server/LIVE.md section 1." % boots)
elif has(r"SIM module: NOT RESPONDING"):
    out("MODEM NOT RESPONDING", "31",
        "The modem never answered AT. It is not powered, not wired to D10/D11,\n"
        "   or the D11 level divider is wrong. Nothing else can be tested first.")
elif has(r"SIM card NOT READY"):
    out("SIM PROBLEM", "31", "The modem is alive but the SIM is not ready.\n"
        "   Reseat it; check it is not PIN-locked.")
elif has(r"network DENIED"):
    out("REGISTRATION DENIED", "31",
        "The network refused this SIM. Plan, account status or roaming.")
elif has(r"network NOT registered") or has(r"searching"):
    out("NOT REGISTERED", "31",
        "The modem is alive and the SIM is ready, but it never joined the\n"
        "   network. Antenna connected? Signal where the board is sitting?")
elif has(r"signal NONE"):
    out("NO SIGNAL", "31", "CSQ reports no signal. Check the antenna.")
elif not has(r"ThingSpeak NODE"):
    out("NO UPLOAD ATTEMPTED", "33",
        "The base ran but never tried to upload -- it only uploads when a NODE\n"
        "   packet arrives over the HC-12 radio. Power on a sensor node, or\n"
        "   check THINGSPEAK_ENABLED is 1.")
else:
    out("INCONCLUSIVE", "33", "Ran without a clear result. Read %s." % sys.argv[1])

print()
m = [l for l in L if "SIM module:" in l or "SIM card" in l]
if m: print("modem status line:\n   " + m[-1])
PY
