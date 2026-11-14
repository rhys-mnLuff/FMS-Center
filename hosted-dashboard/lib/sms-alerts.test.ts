import { describe, expect, it } from "vitest";
import { decideBattery, decideFlood, FLOOD_COOLDOWN_MS, type RecentReading } from "./alerts";
import { sendSms } from "./semaphore";

const site = { name: "Node 2", latLong: "14.5,121.0" };
const r = (o: Partial<RecentReading> = {}): RecentReading =>
  ({ soil: 400, water: 40, rain: 2, risk: 3, batt_pct: 80, batt_ma: 50, lat: null, lon: null, ...o });
const NOW = 10_000_000;

describe("decideFlood", () => {
  it("needs 3 confirmed readings", () => {
    expect(decideFlood([r(), r()], null, NOW, site).send).toBe(false);
    expect(decideFlood([r(), r(), r({ risk: 1 })], null, NOW, site).send).toBe(false);
  });
  it("fires on 3 consecutive warnings and uses the firmware wording", () => {
    const d = decideFlood([r(), r(), r()], null, NOW, site);
    expect(d.send && d.body).toContain("FMS FLOOD ALERT\nNode 2\nWater: FLOODING");
    expect(d.send && d.body).toContain("Loc: 14.5,121.0");
  });
  it("honours the 1 h cooldown", () => {
    expect(decideFlood([r(), r(), r()], NOW - FLOOD_COOLDOWN_MS + 1, NOW, site).send).toBe(false);
    expect(decideFlood([r(), r(), r()], NOW - FLOOD_COOLDOWN_MS, NOW, site).send).toBe(true);
  });
});

describe("decideBattery", () => {
  it("only when critical and discharging", () => {
    expect(decideBattery(r({ batt_pct: 50 }), null, NOW, site).send).toBe(false);
    expect(decideBattery(r({ batt_pct: 10, batt_ma: 20 }), null, NOW, site).send).toBe(false);
    expect(decideBattery(r({ batt_pct: 10, batt_ma: -20 }), null, NOW, site).send).toBe(true);
  });
});

describe("sendSms (Semaphore)", () => {
  it("posts form-encoded to the right route, strips +, maps per-recipient results", async () => {
    const calls: { url: string; form: URLSearchParams }[] = [];
    const f = (async (url: string, init: any) => {
      calls.push({ url, form: init.body });
      return Response.json([
        { message_id: 11, recipient: "639170000000", status: "Pending" },
        { message_id: 12, recipient: "639170000001", status: "Failed" },
      ]);
    }) as unknown as typeof fetch;
    const res = await sendSms({ apiKey: "k", priority: true }, ["+639170000000", "+639170000001", "+639170000002"], "hi", f);
    expect(calls[0].url).toBe("https://api.semaphore.co/api/v4/priority");
    expect(calls[0].form.get("number")).toBe("639170000000,639170000001,639170000002");
    expect(calls[0].form.get("sendername")).toBeNull();
    expect(res.map(r => r.ok)).toEqual([true, false, false]);
    expect(res[0].id).toBe("11");
    expect(res[2].error).toMatch(/no result/);
  });
  it("sends sendername when given and isolates a failed batch", async () => {
    let n = 0;
    const f = (async (_u: string, init: any) => {
      expect(init.body.get("sendername")).toBe("FMS");
      return ++n === 1 ? Response.json({ message: "Insufficient balance" }, { status: 400 })
                       : Response.json([{ message_id: 1, recipient: "639170000002", status: "Queued" }]);
    }) as unknown as typeof fetch;
    const res = await sendSms({ apiKey: "k", senderName: "FMS", batchSize: 2 },
      ["+639170000000", "+639170000001", "+639170000002"], "hi", f);
    expect(res.map(r => r.ok)).toEqual([false, false, true]);
    expect(res[0].error).toBe("Insufficient balance");
  });
});
