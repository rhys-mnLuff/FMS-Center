#!/usr/bin/env python3
"""
FMS ingest + feed server.

Accepts exactly what Base_Code.ino's sendToThingSpeak() sends:

  GET /update?api_key=KEY&field1=soil&field2=water&field3=health
             [&field4=rain&field5=pct&field6=mv&field7=ma]
             [&field8=risk]&lat=LAT&long=LON

and serves it back as JSON for the dashboard:

  GET /feed            all nodes, most recent first
  GET /feed?node=1     one node
  GET /health          liveness

Plain HTTP on purpose. The base station is an ATmega328P talking through a
SIM7600 with AT+HTTPPARA="URL","http://..." -- there is no TLS stack on that
chip, so an HTTPS-only host cannot be used without changing the firmware.

Standard library only: no pip install, runs on any free tier VM.

  python3 fms_server.py [--port 8080] [--db fms.db]
"""

import argparse, json, sqlite3, sys, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

# api_key -> node id. The firmware sends a per-node key; we use it as the
# node's identity so one compromised key can't write as another node.
NODE_KEYS = {
    "O3JGLAD7IX4P67IE": 1,
    "REPLACE_WITH_NODE_2_KEY": 2,
    "REPLACE_WITH_NODE_3_KEY": 3,
}

SCHEMA = """
CREATE TABLE IF NOT EXISTS readings (
  id      INTEGER PRIMARY KEY AUTOINCREMENT,
  node    INTEGER NOT NULL,
  ts      INTEGER NOT NULL,      -- server receive time, ms. The node sends no clock.
  soil    INTEGER, water INTEGER, health INTEGER,
  rain    INTEGER, batt_pct INTEGER, batt_mv INTEGER, batt_ma INTEGER,
  risk    INTEGER, lat REAL, lon REAL
);
CREATE INDEX IF NOT EXISTS idx_node_ts ON readings(node, ts DESC);
"""

db = None


def _int(qs, key):
    """Query values arrive as strings; missing or unparseable becomes None.
    The firmware omits field4-7 entirely when a sensor is absent, and sends
    -1 when it has the sensor but no valid reading. Both mean 'no data'."""
    v = qs.get(key, [None])[0]
    if v is None:
        return None
    try:
        n = int(v)
    except ValueError:
        return None
    return None if n < 0 else n


def _float(qs, key):
    v = qs.get(key, [None])[0]
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def _send(self, code, body=b"", ctype="text/plain"):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        # the dashboard is served from a different port, so it needs this
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        if body:
            self.wfile.write(body)

    def _json(self, code, obj):
        self._send(code, json.dumps(obj).encode(), "application/json")

    def do_GET(self):
        u = urlparse(self.path)
        qs = parse_qs(u.query)

        if u.path == "/update":
            key = qs.get("api_key", [""])[0]
            node = NODE_KEYS.get(key)
            if node is None:
                # Mirrors ThingSpeak: "0" means rejected. The firmware only
                # treats HTTP 200 as success, so a 401 shows up as FAILED.
                self.log_message("rejected unknown api_key %r", key[:8])
                return self._send(401, b"0")

            row = (
                node, int(time.time() * 1000),
                _int(qs, "field1"), _int(qs, "field2"), _int(qs, "field3"),
                _int(qs, "field4"), _int(qs, "field5"), _int(qs, "field6"),
                qs.get("field7", [None])[0] and int(qs["field7"][0]),  # mA may be negative
                _int(qs, "field8"),
                _float(qs, "lat"), _float(qs, "long"),
            )
            db.execute(
                "INSERT INTO readings (node,ts,soil,water,health,rain,"
                "batt_pct,batt_mv,batt_ma,risk,lat,lon) "
                "VALUES (?,?,?,?,?,?,?,?,?,?,?,?)", row)
            db.commit()
            self.log_message("node %s soil=%s water=%s risk=%s",
                             node, row[2], row[3], row[9])
            return self._send(200, b"1")

        if u.path == "/feed":
            node = qs.get("node", [None])[0]
            limit = min(int(qs.get("limit", ["200"])[0]), 2000)
            if node:
                cur = db.execute(
                    "SELECT * FROM readings WHERE node=? ORDER BY ts DESC LIMIT ?",
                    (int(node), limit))
            else:
                cur = db.execute(
                    "SELECT * FROM readings ORDER BY ts DESC LIMIT ?", (limit,))
            cols = [c[0] for c in cur.description]
            return self._json(200, [dict(zip(cols, r)) for r in cur.fetchall()])

        if u.path == "/health":
            n = db.execute("SELECT COUNT(*) FROM readings").fetchone()[0]
            last = db.execute("SELECT MAX(ts) FROM readings").fetchone()[0]
            return self._json(200, {
                "ok": True, "readings": n,
                "last_reading_ms_ago": (int(time.time() * 1000) - last) if last else None,
            })

        self._send(404, b"not found")

    def log_message(self, fmt, *args):
        sys.stderr.write("%s  %s\n" % (time.strftime("%H:%M:%S"), fmt % args))


def main():
    global db
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--db", default="fms.db")
    a = ap.parse_args()

    db = sqlite3.connect(a.db, check_same_thread=False)
    db.executescript(SCHEMA)

    print(f"FMS server on http://0.0.0.0:{a.port}  (db: {a.db})")
    print(f"  ingest: /update?api_key=...&field1=...")
    print(f"  feed:   /feed   health: /health")
    ThreadingHTTPServer(("0.0.0.0", a.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
