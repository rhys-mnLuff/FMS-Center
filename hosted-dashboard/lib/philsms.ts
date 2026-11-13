/**
 * PhilSMS client (https://app.philsms.com/developers/documentation).
 *
 * POST /api/v3/sms/send, Bearer token, JSON body, comma-separated recipients.
 * The docs do not state a rate limit or a batch cap, so recipients are sent in
 * small sequential batches and each batch succeeds or fails on its own: one
 * rejected batch must not stop the other 600 people being warned.
 *
 * Nothing here reads env or the database, so it is testable with a fake fetch.
 */

export interface PhilSmsConfig {
  token: string;
  senderId: string;          // must be approved on the PhilSMS account
  baseUrl?: string;
  batchSize?: number;
}

export interface BatchResult {
  recipients: string[];      // E.164, as passed in
  ok: boolean;
  uid?: string;              // PhilSMS message id, when the response carries one
  error?: string;
}

/** PhilSMS wants 639XXXXXXXXX -- no plus. */
export const toPhilSmsNumber = (e164: string) => e164.replace(/^\+/, "");

export async function sendSms(
  cfg: PhilSmsConfig,
  recipients: string[],
  message: string,
  fetchImpl: typeof fetch = fetch,
): Promise<BatchResult[]> {
  const size = cfg.batchSize ?? 50;
  const url = (cfg.baseUrl ?? "https://app.philsms.com") + "/api/v3/sms/send";
  const results: BatchResult[] = [];
  for (let i = 0; i < recipients.length; i += size) {
    const batch = recipients.slice(i, i + size);
    try {
      const res = await fetchImpl(url, {
        method: "POST",
        headers: {
          Authorization: `Bearer ${cfg.token}`,
          "Content-Type": "application/json",
          Accept: "application/json",
        },
        body: JSON.stringify({
          recipient: batch.map(toPhilSmsNumber).join(","),
          sender_id: cfg.senderId,
          type: "plain",
          message,
        }),
      });
      const body = (await res.json().catch(() => null)) as
        { status?: string; message?: string; data?: { uid?: string } } | null;
      if (res.ok && body?.status === "success") {
        results.push({ recipients: batch, ok: true, uid: body.data?.uid });
      } else {
        results.push({ recipients: batch, ok: false, error: body?.message ?? `HTTP ${res.status}` });
      }
    } catch (e) {
      results.push({ recipients: batch, ok: false, error: e instanceof Error ? e.message : "network" });
    }
  }
  return results;
}
