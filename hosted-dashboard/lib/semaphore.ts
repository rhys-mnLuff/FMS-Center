/**
 * Semaphore client (https://api.semaphore.co/docs).
 *
 * POST /api/v4/messages or /api/v4/priority, apikey in the form body,
 * comma-separated numbers (up to 1000 per call). /messages is limited to 120
 * calls a minute; /priority is not rate limited but costs 2 credits per SMS,
 * so it is used for flood alerts, where a throttled batch is not acceptable.
 *
 * `sendername` is optional: omitted, Semaphore uses the account's default
 * sender name, so no approval step is needed in code.
 *
 * Nothing here reads env or the database, so it is testable with a fake fetch.
 * Each batch succeeds or fails alone: one rejected batch must not stop the
 * other 600 people being warned.
 */

export interface SemaphoreConfig {
  apiKey: string;
  senderName?: string;
  priority?: boolean;
  baseUrl?: string;
  batchSize?: number;
}

export interface SendResult {
  msisdn: string;            // E.164, as passed in
  ok: boolean;
  id?: string;               // Semaphore message_id, for status lookup
  error?: string;
}

/** Semaphore takes 639XXXXXXXXX or 09XXXXXXXXX; use the former. */
export const toSemaphoreNumber = (e164: string) => e164.replace(/^\+/, "");

const last10 = (s: string) => s.replace(/\D/g, "").slice(-10);

interface SemaphoreMessage { message_id?: number | string; recipient?: string; status?: string }

export async function sendSms(
  cfg: SemaphoreConfig,
  recipients: string[],
  message: string,
  fetchImpl: typeof fetch = fetch,
): Promise<SendResult[]> {
  const size = cfg.batchSize ?? 500;
  const url = `${cfg.baseUrl ?? "https://api.semaphore.co"}/api/v4/${cfg.priority ? "priority" : "messages"}`;
  const out: SendResult[] = [];
  for (let i = 0; i < recipients.length; i += size) {
    const batch = recipients.slice(i, i + size);
    const form = new URLSearchParams({ apikey: cfg.apiKey, number: batch.map(toSemaphoreNumber).join(","), message });
    if (cfg.senderName) form.set("sendername", cfg.senderName);
    try {
      const res = await fetchImpl(url, { method: "POST", body: form, headers: { Accept: "application/json" } });
      const body = (await res.json().catch(() => null)) as SemaphoreMessage[] | { message?: string; error?: string } | null;
      if (!res.ok || !Array.isArray(body)) {
        const err = (!Array.isArray(body) && (body?.message || body?.error)) || `HTTP ${res.status}`;
        for (const msisdn of batch) out.push({ msisdn, ok: false, error: String(err) });
        continue;
      }
      const byNumber = new Map(body.map(m => [last10(String(m.recipient ?? "")), m]));
      for (const msisdn of batch) {
        const m = byNumber.get(last10(msisdn));
        const failed = m?.status === "Failed" || m?.status === "Refunded";
        out.push(m ? { msisdn, ok: !failed, id: m.message_id !== undefined ? String(m.message_id) : undefined, error: failed ? m.status : undefined }
                   : { msisdn, ok: false, error: "no result returned for this number" });
      }
    } catch (e) {
      for (const msisdn of batch) out.push({ msisdn, ok: false, error: e instanceof Error ? e.message : "network" });
    }
  }
  return out;
}
