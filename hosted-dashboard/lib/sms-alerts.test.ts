import { describe, expect, it } from "vitest";
import { decideBattery, decideFlood, FLOOD_COOLDOWN_MS, type RecentReading } from "./alerts";
import { sendSms } from "./philsms";

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

describe("sendSms", () => {
  it("batches, strips +, and isolates a failed batch", async () => {
    const calls: any[] = [];
    const f = (async (_u: string, init: any) => {
      const body = JSON.parse(init.body); calls.push(body);
      return calls.length === 1
        ? Response.json({ status: "error", message: "bad sender" }, { status: 422 })
        : Response.json({ status: "success", data: { uid: "abc" } });
    }) as unknown as typeof fetch;
    const nums = Array.from({ length: 3 }, (_, i) => `+63917000000${i}`);
    const res = await sendSms({ token: "t", senderId: "FMS", batchSize: 2 }, nums, "hi", f);
    expect(calls[0].recipient).toBe("639170000000,639170000001");
    expect(res.map(x => x.ok)).toEqual([false, true]);
    expect(res[0].error).toBe("bad sender");
  });
});
