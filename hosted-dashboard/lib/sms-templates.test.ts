import { describe, expect, it } from "vitest";
import {
  buildFloodSms, buildLowBatterySms, soilWords, rainWords, riskWords,
  smsParts, withOptOut, OPT_OUT_SUFFIX,
} from "./sms-templates";

/**
 * These pin the wording against Base_Code.ino. They are not testing that the
 * code runs -- they are testing that an admin's handset and a farmer's phone
 * say the same thing. If the firmware's templates change, these fail, and
 * that failure is the point.
 */

describe("soilWords matches the firmware bands", () => {
  it.each([
    [1009, "no reading"],   // >= RISK_SOIL_FAULT_FROM (probe out of soil)
    [960,  "no reading"],
    [959,  "very dry"],
    [829,  "very dry"],
    [828,  "dry"],
    [700,  "dry"],
    [699,  "normal"],
    [552,  "normal"],
    [551,  "wet"],
    [437,  "wet"],
    [436,  "SATURATED"],
    [349,  "SATURATED"],
  ])("soil %i -> %s", (soil, want) => expect(soilWords(soil)).toBe(want));
});

describe("rainWords / riskWords", () => {
  it("rain", () => {
    expect(rainWords(0)).toBe("no rain");
    expect(rainWords(1)).toBe("light rain");
    expect(rainWords(2)).toBe("heavy rain");
    expect(rainWords(3)).toBe("no reading");
    expect(rainWords(null)).toBe("no reading");
  });
  it("risk", () => {
    expect(riskWords(0)).toBe("NORMAL");
    expect(riskWords(1)).toBe("WATCH - keep an eye out");
    expect(riskWords(2)).toBe("WARNING - flood possible");
    expect(riskWords(3)).toBe("FLOODING NOW");
  });
});

describe("buildFloodSms is byte-identical to the firmware", () => {
  it("standing water -> ALERT", () => {
    expect(buildFloodSms({
      siteName: "North Rice Paddy", soil: 420, water: 45, rain: 2,
      latLong: "14.5333,121.0000",
    })).toBe(
      "FMS FLOOD ALERT\n" +
      "North Rice Paddy\n" +
      "Water: FLOODING\n" +
      "Soil: SATURATED\n" +
      "Rain: heavy rain\n" +
      "Act now: move to higher ground\n" +
      "Loc: 14.5333,121.0000"
    );
  });

  it("no standing water yet -> WARNING", () => {
    expect(buildFloodSms({
      siteName: "East Canal Field", soil: 500, water: 10, rain: 1,
      latLong: "14.5333,121.0000",
    })).toBe(
      "FMS FLOOD WARNING\n" +
      "East Canal Field\n" +
      "Water: none yet\n" +
      "Soil: wet\n" +
      "Rain: light rain\n" +
      "Flood likely soon. Get ready\n" +
      "Loc: 14.5333,121.0000"
    );
  });

  it("water exactly at the threshold is not yet flooding", () => {
    // Firmware: water > BRIEF_WATER_ALERT_ABOVE, so 30 is NOT an alert.
    expect(buildFloodSms({ siteName: "S", soil: 500, water: 30, rain: 0,
      latLong: "0,0" })).toContain("FMS FLOOD WARNING");
    expect(buildFloodSms({ siteName: "S", soil: 500, water: 31, rain: 0,
      latLong: "0,0" })).toContain("FMS FLOOD ALERT");
  });

  it("never contains a link", () => {
    // Philippine networks were observed rejecting texts with links
    // (SMS_MAP_LINK 0 in the firmware). A regression here is undeliverable.
    const s = buildFloodSms({ siteName: "S", soil: 400, water: 50, rain: 2,
      latLong: "14.5333,121.0000", includeRaw: true });
    expect(s).not.toMatch(/https?:\/\//);
    expect(s).not.toContain("maps.google.com");
    expect(s).toContain("Loc: 14.5333,121.0000");
  });

  it("appends raw values only when asked", () => {
    expect(buildFloodSms({ siteName: "S", soil: 400, water: 50, rain: 2,
      latLong: "0,0", includeRaw: true })).toContain("\nRaw s400 w50");
    expect(buildFloodSms({ siteName: "S", soil: 400, water: 50, rain: 2,
      latLong: "0,0" })).not.toContain("Raw");
  });
});

describe("buildLowBatterySms", () => {
  it("matches the firmware", () => {
    expect(buildLowBatterySms({ siteName: "North Rice Paddy", pct: 12 })).toBe(
      "FMS BATTERY LOW\n" +
      "North Rice Paddy\n" +
      "Battery: 12% and draining\n" +
      "Sensor may stop soon. Check the solar panel is clean and in sun"
    );
  });
});

describe("opt-out costs money, so only add it when it is free", () => {
  it("a real flood alert still fits in one part with the suffix", () => {
    const body = buildFloodSms({ siteName: "North Rice Paddy", soil: 420,
      water: 45, rain: 2, latLong: "14.5333,121.0000" });
    expect(smsParts(body)).toBe(1);
    const out = withOptOut(body);
    expect(out).toContain(OPT_OUT_SUFFIX);
    expect(smsParts(out)).toBe(1);   // 700 recipients, so a 2nd part doubles the bill
  });

  it("is dropped rather than pushing into a second part", () => {
    const long = "x".repeat(155);
    expect(smsParts(long)).toBe(1);
    expect(withOptOut(long)).toBe(long);      // suffix would spill, so omitted
  });
});
