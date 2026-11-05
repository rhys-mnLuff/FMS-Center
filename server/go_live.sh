#!/usr/bin/env bash
#
# One command from "module plugged in" to "readings on the dashboard".
#
# A Cloudflare quick tunnel gets a NEW random hostname every launch, and
# Base_Code.ino has that hostname compiled in (SERVER_HOST). So every tunnel
# restart needs a source edit, a recompile and a reflash. Doing that by hand
# is how the endpoint ends up pointing at a tunnel that died days ago --
# which is exactly the state this repo was in.
#
#   ./server/go_live.sh                 # full run: server, tunnel, flash, watch
#   ./server/go_live.sh --no-flash      # leave the board alone (prints the host)
#   ./server/go_live.sh --port /dev/cu.usbserial-1130
#
# Requires: python3, cloudflared, arduino-cli. Nothing else.
#
# NOTE: this is the stop-gap. A quick tunnel is not a deployment -- it dies
# with this laptop and the hostname is thrown away. See MAKING_IT_REAL.md
# section 5 for the permanent endpoint.

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH="$REPO/Base_Code.ino"
FQBN_DEFAULT="arduino:avr:nano:cpu=atmega328old"
PORT=""
FLASH=1

while [ $# -gt 0 ]; do
  case "$1" in
    --no-flash) FLASH=0; shift ;;
    --port)     PORT="$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

say(){ printf '\n\033[1m==> %s\033[0m\n' "$1"; }
die(){ printf '\033[31merror: %s\033[0m\n' "$1" >&2; exit 1; }

# --- 1. ingest server ------------------------------------------------------
say "ingest server"
if lsof -nP -iTCP:8080 -sTCP:LISTEN >/dev/null 2>&1; then
  echo "    already listening on :8080"
else
  ( cd "$REPO/server" && nohup python3 fms_server.py >/tmp/fms_server.log 2>&1 & )
  sleep 1.5
  lsof -nP -iTCP:8080 -sTCP:LISTEN >/dev/null 2>&1 || die "server did not start; see /tmp/fms_server.log"
  echo "    started (log: /tmp/fms_server.log)"
fi
curl -fsS http://localhost:8080/health >/dev/null || die "/health did not answer"

# --- 2. tunnel -------------------------------------------------------------
say "cloudflare quick tunnel"
TLOG=/tmp/fms_tunnel.log
pkill -f "cloudflared tunnel --url http://localhost:8080" 2>/dev/null || true
: > "$TLOG"
nohup cloudflared tunnel --url http://localhost:8080 >"$TLOG" 2>&1 &
HOST=""
for _ in $(seq 1 40); do
  HOST=$(grep -oE '[a-z0-9-]+\.trycloudflare\.com' "$TLOG" | head -1 || true)
  [ -n "$HOST" ] && break
  sleep 1
done
[ -n "$HOST" ] || die "no tunnel hostname after 40s; see $TLOG"
echo "    $HOST"
# Prove it end to end before committing it to firmware: a hostname that
# resolves but does not reach our server would flash a dead endpoint.
curl -fsS --max-time 20 "http://$HOST/health" >/dev/null \
  || die "tunnel is up but http://$HOST/health did not answer"
echo "    verified: http://$HOST/health reachable from outside"

# --- 3. patch the sketch ---------------------------------------------------
say "firmware endpoint"
CURRENT=$(grep -oE 'SERVER_HOST\[\] = "[^"]*"' "$SKETCH" | sed 's/.*"\(.*\)"/\1/')
if [ "$CURRENT" = "$HOST" ]; then
  echo "    unchanged ($HOST)"
else
  python3 - "$SKETCH" "$HOST" <<'PY'
import re, sys
path, host = sys.argv[1], sys.argv[2]
src = open(path, encoding="utf-8").read()
new, n = re.subn(r'(SERVER_HOST\[\] = ")[^"]*(")', lambda m: m.group(1)+host+m.group(2), src)
if n != 1:
    sys.exit("expected exactly one SERVER_HOST line, found %d" % n)
open(path, "w", encoding="utf-8").write(new)
PY
  echo "    $CURRENT"
  echo " -> $HOST"
fi

if [ "$FLASH" = "0" ]; then
  say "done (--no-flash)"
  echo "    endpoint: http://$HOST/update"
  echo "    dashboard feed: http://$HOST/feed"
  exit 0
fi

# --- 4. compile and upload -------------------------------------------------
say "board"
if [ -z "$PORT" ]; then
  PORT=$(ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.usbmodem* 2>/dev/null | head -1 || true)
fi
[ -n "$PORT" ] || die "no board found. Plug it in, or pass --port /dev/cu.something"
echo "    $PORT"

# The serial bridge holds the port open and the upload will fail on a busy
# port with a confusing avrdude error, so say what is actually wrong.
if pgrep -f "serial_bridge.py" >/dev/null 2>&1; then
  die "serial_bridge.py is running and holding $PORT. Stop it first (pkill -f serial_bridge.py)."
fi

BUILD=$(mktemp -d)/Base_Code
mkdir -p "$BUILD" && cp "$SKETCH" "$BUILD/Base_Code.ino"

say "compile"
arduino-cli compile --fqbn "$FQBN_DEFAULT" "$BUILD" | tail -3

say "upload"
# Nano boards ship with either bootloader; the new one is the common case on
# recent clones, the "old" one on genuine and older boards. Try, then fall back.
if ! arduino-cli upload -p "$PORT" --fqbn "$FQBN_DEFAULT" "$BUILD" 2>/tmp/fms_upload.log; then
  echo "    old-bootloader upload failed, retrying at the newer baud rate"
  arduino-cli compile --fqbn arduino:avr:nano "$BUILD" >/dev/null
  arduino-cli upload -p "$PORT" --fqbn arduino:avr:nano "$BUILD" \
    || { cat /tmp/fms_upload.log >&2; die "upload failed on both bootloaders"; }
fi
echo "    flashed"

# --- 5. wait for a reading -------------------------------------------------
say "waiting for the first cellular reading"
BEFORE=$(curl -fsS http://localhost:8080/health | python3 -c 'import json,sys; print(json.load(sys.stdin)["readings"])')
echo "    $BEFORE readings stored before this run"
echo "    the modem registers on the network first; allow ~60s"
for i in $(seq 1 24); do
  sleep 5
  NOW=$(curl -fsS http://localhost:8080/health | python3 -c 'import json,sys; print(json.load(sys.stdin)["readings"])')
  if [ "$NOW" -gt "$BEFORE" ]; then
    say "reading received"
    curl -fsS "http://localhost:8080/feed?limit=1" | python3 -m json.tool
    echo
    echo "dashboard:  http://localhost:8766/dev.html   (press 'Live feed')"
    echo "public feed: http://$HOST/feed"
    exit 0
  fi
  printf '.'
done

echo
die "no reading in 2 minutes.
     Watch the board's own log for the reason:
       arduino-cli monitor -p $PORT -c baudrate=9600
     'FAILED (HTTP 6xx)'  -> no data connection: APN or data plan
     board resets mid-send -> power. The modem needs ~2A peaks; a laptop USB
                              port supplies 0.5-0.9A. See HARDWARE.md."
