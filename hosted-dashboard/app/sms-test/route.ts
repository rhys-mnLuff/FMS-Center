import { env } from "cloudflare:workers";
import { sendSms } from "../../lib/philsms";
import { normalisePhNumber } from "../../lib/subscribers";
import { matchesKey } from "../../lib/reading";

/**
 * POST /sms-test?key=<SMS_ADMIN_KEY>
 * Sends one test text to ADMIN_NUMBERS only, never to subscribers. Run it
 * after setup: it proves the token, the approved sender name and the numbers
 * are right before any real alert depends on them.
 */
export async function POST(request: Request) {
  const key = new URL(request.url).searchParams.get("key") ?? "";
  if (!matchesKey(key, env.SMS_ADMIN_KEY)) return new Response("unauthorized", { status: 401 });
  if (!env.PHILSMS_TOKEN || !env.PHILSMS_SENDER_ID) return Response.json({ error: "PHILSMS_TOKEN / PHILSMS_SENDER_ID not set" }, { status: 503 });
  const to = (env.ADMIN_NUMBERS ?? "").split(",").map(n => normalisePhNumber(n.trim())).filter((n): n is string => !!n);
  if (!to.length) return Response.json({ error: "ADMIN_NUMBERS is empty or invalid" }, { status: 503 });
  const results = await sendSms({ token: env.PHILSMS_TOKEN, senderId: env.PHILSMS_SENDER_ID }, to,
    "FMS test message. If you can read this, flood alerts are set up correctly.");
  return Response.json({ sent_to: to.length, ok: results.every(r => r.ok), errors: results.filter(r => !r.ok).map(r => r.error) });
}
