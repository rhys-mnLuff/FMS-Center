import { env } from "cloudflare:workers";
import { sendSms } from "../../lib/semaphore";
import { normalisePhNumber } from "../../lib/subscribers";
import { matchesKey } from "../../lib/reading";

/**
 * POST /sms-test?key=<SMS_ADMIN_KEY>
 * Sends one test text to ADMIN_NUMBERS only, never to subscribers. Run it
 * after setup: it proves the API key, the sender name and the numbers
 * are right before any real alert depends on them.
 */
export async function POST(request: Request) {
  const key = new URL(request.url).searchParams.get("key") ?? "";
  if (!matchesKey(key, env.SMS_ADMIN_KEY)) return new Response("unauthorized", { status: 401 });
  if (!env.SEMAPHORE_API_KEY) return Response.json({ error: "SEMAPHORE_API_KEY not set" }, { status: 503 });
  const to = (env.ADMIN_NUMBERS ?? "").split(",").map(n => normalisePhNumber(n.trim())).filter((n): n is string => !!n);
  if (!to.length) return Response.json({ error: "ADMIN_NUMBERS is empty or invalid" }, { status: 503 });
  const results = await sendSms({ apiKey: env.SEMAPHORE_API_KEY, senderName: env.SEMAPHORE_SENDER_NAME || undefined }, to,
    "FMS test message. If you can read this, flood alerts are set up correctly.");
  return Response.json({ sent_to: to.length, ok: results.every(r => r.ok), errors: [...new Set(results.filter(r => !r.ok).map(r => r.error))] });
}
