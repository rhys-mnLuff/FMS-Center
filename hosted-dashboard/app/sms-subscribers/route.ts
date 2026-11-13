import { env } from "cloudflare:workers";
import { normalisePhNumber } from "../../lib/subscribers";
import { matchesKey } from "../../lib/reading";

/**
 * Bulk-load a zone's subscribers.
 *
 *   curl -X POST "$HOST/sms-subscribers?key=$SMS_ADMIN_KEY" \
 *     -H 'content-type: application/json' \
 *     -d '{"zone":2,"consent":true,"source":"barangay sign-up sheet 14 Nov","numbers":["0917 123 4567", ...]}'
 *
 * `consent: true` is required and recorded as consent_ts: it is your statement
 * that these people agreed to receive flood alerts (Data Privacy Act). Numbers
 * that already exist are left alone, so re-importing never re-activates
 * someone who texted STOP.
 */
export async function POST(request: Request) {
  const key = new URL(request.url).searchParams.get("key") ?? "";
  if (!env.DB || !matchesKey(key, env.SMS_ADMIN_KEY)) return new Response("unauthorized", { status: 401 });
  const body = (await request.json().catch(() => null)) as
    { zone?: number; consent?: boolean; source?: string; numbers?: string[] } | null;
  const zone = body?.zone;
  if (!Number.isInteger(zone) || zone! < 1 || zone! > 254 || !Array.isArray(body?.numbers) || body!.numbers!.length > 5000)
    return Response.json({ error: "need zone (1-254) and numbers[] (max 5000)" }, { status: 400 });
  if (body!.consent !== true)
    return Response.json({ error: "consent must be true: confirm these people agreed to receive alerts" }, { status: 400 });

  const valid = new Set<string>(); const invalid: string[] = [];
  for (const raw of body!.numbers!) {
    const n = normalisePhNumber(String(raw));
    if (n) valid.add(n); else invalid.push(String(raw).slice(0, 4) + "…");   // never echo a full number back
  }
  const now = Date.now();
  const stmts = [...valid].map(n => env.DB!.prepare(
    "INSERT OR IGNORE INTO subscribers (zone,msisdn,status,consent_ts,source,created_ts) VALUES (?,?,'active',?,?,?)",
  ).bind(zone, n, now, body!.source ?? "bulk import", now));
  const res = stmts.length ? await env.DB.batch(stmts) : [];
  const added = res.reduce((s, r) => s + (r.meta.changes ?? 0), 0);
  return Response.json({ zone, added, already_present: valid.size - added, invalid_count: invalid.length, invalid });
}
