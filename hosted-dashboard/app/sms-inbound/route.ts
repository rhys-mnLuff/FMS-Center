import { env } from "cloudflare:workers";
import { isStopMessage, normalisePhNumber } from "../../lib/subscribers";
import { matchesKey } from "../../lib/reading";

/**
 * Inbound STOP. PhilSMS documents no inbound webhook, so nothing calls this
 * yet -- it exists for whichever forwarder delivers replies (a PhilSMS
 * webhook if support enables one, or a gateway phone). Guarded by a secret.
 *
 * POST /sms-inbound?key=...   JSON {"from": "+639...", "message": "STOP"}
 * STOP applies to the person: every zone row for that number is stopped.
 */
export async function POST(request: Request) {
  const key = new URL(request.url).searchParams.get("key") ?? "";
  if (!env.DB || !matchesKey(key, env.SMS_INBOUND_KEY)) return new Response("unauthorized", { status: 401 });
  const body = (await request.json().catch(() => null)) as { from?: string; message?: string } | null;
  const from = normalisePhNumber(body?.from ?? "");
  if (!from || !isStopMessage(body?.message ?? "")) return Response.json({ stopped: 0 });
  const r = await env.DB.prepare(
    "UPDATE subscribers SET status='stopped', stopped_ts=? WHERE msisdn=? AND status='active'",
  ).bind(Date.now(), from).run();
  return Response.json({ stopped: r.meta.changes });
}
