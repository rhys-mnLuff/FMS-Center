#!/usr/bin/env python3
"""
USB serial -> FMS ingest bridge.

Reads the base station's Serial Monitor output over USB and forwards each
reading to the same /update endpoint the cellular path uses. Nothing
downstream can tell the difference, so the dashboard wiring done against
this keeps working unchanged once THINGSPEAK_ENABLED is turned on.

Lines it understands (printPacketLine / printPowerLine in Base_Code.ino):

  NODE 1 | hops 0 | soil 1019 | water 0 | rain NONE | batt 18% discharging LOW | risk NORMAL
  NODE 1 power | 3.72V | 10-min avg -76mA | est. 5.2h left

The power line arrives separately from the reading, so battery values are
cached per node and attached to the next reading.

Needs no cellular, no SIM credit and no internet. Standard library only.

  python3 serial_bridge.py --port /dev/cu.usbserial-1130
"""

import argparse, re, sys, time, urllib.parse, urllib.request

# Reading line. rain is a word here (NONE/LIGHT/HEAVY) -- the raw ADC value
# only appears in the 'n' node-table output, so it is deliberately not
# forwarded rather than invented from the class.
RE_READING = re.compile(
    r"NODE\s+(\d+)\s*\|\s*hops\s+(\d+)\s*\|\s*soil\s+(-?\d+)\s*\|\s*water\s+(-?\d+)"
    r"\s*\|\s*rain\s+(\w+)\s*\|\s*batt\s+([^|]+?)(?:\s*\|\s*risk\s+(\w+))?\s*$"
)
RE_POWER = re.compile(
    r"NODE\s+(\d+)\s+power\s*\|\s*([\d.]+)V\s*\|\s*10-min avg\s+(-?\d+)mA"
)
RE_BATT_PCT = re.compile(r"(\d+)%")

RISK = {"NORMAL": 0, "WATCH": 1, "WARNING": 2, "FLOOD": 3}

power_cache = {}   # node -> (mv, ma)


def post(endpoint, key, node, soil, water, risk, lat, lon, verbose):
    q = {"api_key": key, "field1": soil, "field2": water}
    # field3 is the health index. The dashboard derives it from soil with the
    # same formula, so sending a second copy would just be a thing to drift.
    if risk is not None:
        q["field8"] = risk
    mv_ma = power_cache.get(node)
    if mv_ma:
        mv, ma = mv_ma
        q["field6"], q["field7"] = mv, ma
    q["lat"], q["long"] = lat, lon
    url = endpoint + "?" + urllib.parse.urlencode(q)
    try:
        with urllib.request.urlopen(url, timeout=5) as r:
            ok = r.status == 200
    except Exception as e:
        print(f"  ! post failed: {e}", file=sys.stderr)
        return False
    if verbose:
        print(f"  -> node {node} soil={soil} water={water} risk={risk} "
              f"{'ok' if ok else 'rejected'}")
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbserial-1130")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--endpoint", default="http://localhost:8080/update")
    ap.add_argument("--key", default="O3JGLAD7IX4P67IE")
    ap.add_argument("--lat", default="14.5333")
    ap.add_argument("--lon", default="121.0000")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()

    # stty rather than pyserial: no pip install on a fresh machine.
    import subprocess
    subprocess.run(["stty", "-f", a.port, str(a.baud), "raw", "-echo"], check=True)

    print(f"bridging {a.port} -> {a.endpoint}")
    print("(opening the port resets the board; first readings arrive after its SIM check)\n")

    sent = 0
    with open(a.port, "r", errors="replace") as ser:
        for line in ser:
            line = line.rstrip("\r\n")
            if not line:
                continue

            m = RE_POWER.search(line)
            if m:
                node, volts, ma = int(m.group(1)), float(m.group(2)), int(m.group(3))
                power_cache[node] = (int(volts * 1000), ma)
                if not a.quiet:
                    print(f"[power] node {node}  {volts}V  {ma}mA")
                continue

            m = RE_READING.search(line)
            if m:
                node = int(m.group(1))
                soil, water = int(m.group(3)), int(m.group(4))
                risk = RISK.get((m.group(7) or "").upper())
                if not a.quiet:
                    print(f"[read ] {line}")
                if post(a.endpoint, a.key, node, soil, water, risk,
                        a.lat, a.lon, not a.quiet):
                    sent += 1
                continue

            if not a.quiet and line.strip():
                print(f"[info ] {line}")

    print(f"\nport closed. forwarded {sent} readings.")


if __name__ == "__main__":
    main()
