import { env, waitUntil } from "cloudflare:workers";
import { runAlerts } from "./alert-runner";
import { integer, matchesKey, parseReading } from "./reading";
const headers = { "Cache-Control": "no-store", "Access-Control-Allow-Origin": "*" };
const json = (body: unknown, status = 200) => Response.json(body, { status, headers });
export async function update(request: Request) {
  if (!env.DB || !env.NODE_1_WRITE_KEY) return json({ error: "not_configured" }, 503);
  const q = new URL(request.url).searchParams;
  const key = q.get("api_key") ?? "";
  const keys = [env.NODE_1_WRITE_KEY, env.NODE_2_WRITE_KEY, env.NODE_3_WRITE_KEY];
  const node = keys.findIndex(expected => matchesKey(key, expected)) + 1;
  if (!node) return new Response("0", { status: 401, headers });
  let reading;
  try { reading = parseReading(q); }
  catch { return json({ error: "invalid_reading" }, 400); }
  try {
    const { ts, received_ts, soil, water, health, rain, batt_pct, batt_mv, batt_ma, risk, lat, lon, sample_id } = reading;
    const stored = await env.DB.prepare(`INSERT OR IGNORE INTO readings
      (node,ts,received_ts,soil,water,health,rain,batt_pct,batt_mv,batt_ma,risk,lat,lon,sample_id)
      VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)`).bind(node,ts,received_ts,soil,water,health,rain,batt_pct,batt_mv,batt_ma,risk,lat,lon,sample_id).run();
    // Only a genuinely new reading can trigger a text; a retried duplicate is ignored.
    // waitUntil: the node is on a cellular link and must get its "1" without waiting for SMS.
    if (stored.meta.changes > 0) waitUntil(runAlerts(node));
    return new Response("1", { headers });
  } catch { return json({ error: "storage_unavailable" }, 503); }
}
export async function feed(request: Request) {
  if (!env.DB) return json({ error: "not_configured" }, 503);
  let node, limit;
  try {
    const q = new URL(request.url).searchParams;
    node = integer(q, "node", 1, 254); limit = integer(q, "limit", 1, 2000) ?? 200;
  } catch { return json({ error: "invalid_query" }, 400); }
  try {
    const statement = node
      ? env.DB.prepare("SELECT * FROM readings WHERE node=? ORDER BY id DESC LIMIT ?").bind(node,limit)
      : env.DB.prepare("SELECT * FROM readings ORDER BY id DESC LIMIT ?").bind(limit);
    return json((await statement.all()).results);
  } catch { return json({ error: "storage_unavailable" }, 503); }
}
export async function health() {
  if (!env.DB) return json({ ok: false, error: "not_configured" }, 503);
  try {
    const row = await env.DB.prepare("SELECT COUNT(*) AS n,MAX(ts) AS last,MAX(received_ts) AS received FROM readings").first<{n:number;last:number|null;received:number|null}>();
    return json({ ok: true, readings: row?.n ?? 0,
      last_reading_ms_ago: row?.last ? Date.now() - row.last : null,
      last_received_ms_ago: row?.received ? Date.now() - row.received : null });
  } catch { return json({ ok: false, error: "storage_unavailable" }, 503); }
}
