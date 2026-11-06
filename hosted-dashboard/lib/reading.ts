export function integer(q: URLSearchParams, key: string, min: number, max: number, required = false): number | null {
  const raw = q.get(key);
  if (raw === null || raw === "") {
    if (required) throw new Error(`Missing ${key}`);
    return null;
  }
  if (!/^-?\d+$/.test(raw)) throw new Error(`Invalid ${key}`);
  const value = Number(raw);
  if (!Number.isSafeInteger(value) || value < min || value > max) throw new Error(`Invalid ${key}`);
  return value;
}
function coordinate(q: URLSearchParams, key: string, bound: number) {
  const raw = q.get(key);
  if (raw === null || raw === "") return null;
  const n = Number(raw);
  if (!Number.isFinite(n) || Math.abs(n) > bound) throw new Error(`Invalid ${key}`);
  return n;
}
export function parseReading(q: URLSearchParams, now = Date.now()) {
  const age = integer(q, "age_ms", 0, 604800000) ?? 0;
  const sample = q.get("sample_id");
  if (sample !== null && !/^[a-zA-Z0-9:_-]{1,80}$/.test(sample)) throw new Error("Invalid sample_id");
  return {
    ts: now - age, received_ts: now,
    soil: integer(q, "field1", 0, 1023, true)!, water: integer(q, "field2", 0, 1023, true)!,
    health: integer(q, "field3", 0, 100), rain: integer(q, "field4", 0, 1023),
    batt_pct: integer(q, "field5", 0, 100), batt_mv: integer(q, "field6", 0, 30000),
    batt_ma: integer(q, "field7", -9999, 9999), risk: integer(q, "field8", 0, 3),
    lat: coordinate(q, "lat", 90), lon: coordinate(q, "long", 180), sample_id: sample,
  };
}
export function matchesKey(a: string, b: string | undefined): boolean {
  if (!b || b.length < 24 || a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < b.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}
