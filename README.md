# FMS-Center
Decentralized flood monitoring via solar-powered RF mesh network
# FMS: Decentralized Flood Monitoring System

## Problem

Flash floods destroy agricultural investments faster than farmers can respond. 
Traditional monitoring requires cellular subscriptions for every sensor ($50+/month 
each) and wired infrastructure that's impractical in muddy, remote fields.

## Solution

FMS deploys a network of autonomous solar-powered sensor nodes. Each node 
monitors soil saturation and water level, communicating wirelessly to a central 
base station with a single cellular SIM. No per-sensor subscriptions. No wiring. 
SMS alerts arrive in seconds with geolocation.

## Key Features

- **HC-12 433MHz mesh network**: 340m range (measured), multi-hop relay, no WiFi 
  or local internet required
- **Solar powered**: trickle-charged LiPo cells, runs through monsoon cloud cover
- **Automated alerts**: SMS + GPS coordinates when thresholds are crossed
- **Cloud dashboard**: Next.js PWA hosted on Vercel, free tier compatible
- **Unit economics**: $120/node vs $1,000+ commercial alternatives
- **Field-proven**: deployed in rice paddies, irrigation canals, and river monitoring

## Hardware

- Arduino-class microcontroller
- HC-12 long-range radio module
- SIM7600 cellular + GPS module
- Soil moisture sensor (capacitive v1.2 recommended)
- Parallel-trace surface water level detector
- TP4056 solar charge controller
- 20W solar panel
- IP65 weatherproof enclosure

See `hardware/` for BOM, wiring diagrams, and assembly notes.

## Firmware

- `base_module/`: cellular gateway, alert routing, GPS parsing
- `sensor_node/`: multi-hop relay, sensor polling, power gating
- `moisture_calibration/`: 3-point field calibration protocol
- `moisture_alert_logic/`: debounce, hysteresis, rate-of-rise detection

All sketches: Arduino C++, tested on ATmega328.

Known issues and fixes in `FIRMWARE_NOTES.md`.

## Dashboard

Next.js 14 + Supabase + Vercel. Hosted at `dash.luffmans.com`.

Public demo: `/demo` (all farms visible)
Farm view: `/farm/[access_code]` (farmer login)
Data export: CSV per node for research

See `dashboard/` for schema, deployment, and seed scripts.

## Competition Status

| Event | Deadline | Target | Status |
|---|---|---|---|
| Congressional App Challenge | Oct 2026 | 3 min video | In development |
| National STEM Challenge | Nov 2026 | 2 min video | In development |
| The Earth Prize | Jan 2027 | 3 min video | In development |
| NextUp Founders | Mar 2027 | 5 min video | Planned |
| Blue Ocean Entrepreneurship | Feb 2027/2028 | 5 min video | Planned |

## Deployments & Partnerships

- **Field testing**: Maejo University (Chiang Mai) — pilot with student researchers
- **Institutional interest**: Philippine Disaster Resilience Foundation (PDRF)
- **IP filing**: Philippine utility model in progress (IPOPHL)

## Getting Started

### Local Development

```bash
git clone https://github.com/rhys-luffman/fms.git
cd fms

# Hardware wiring: see hardware/WIRING.md
# Firmware: load base_module.ino to base, sensor_node.ino to nodes
# Dashboard: see dashboard/README.md for Supabase setup and Vercel deploy
```

### Field Deployment

See `docs/DEPLOYMENT.md` for:
- Site survey checklist
- Solar panel positioning
- Soil calibration protocol (30 minutes per site)
- Alert threshold configuration

## Known Limitations & Open Work

- **TLS verification needed**: SIM7600 HTTPS support not yet tested with Supabase
- **Mesh routing**: currently implements bounce-and-dedup; full multi-hop with 
  TTL and CRC8 dedup is in progress
- **Range**: measured at 340m in saturated rice paddies at 1200bps. Fresnel 
  losses at ground level are significant.
- **Calibration**: soil thresholds are site-specific and require the 3-point 
  protocol for accuracy

See `ROADMAP.md` for prioritized fixes and next features.

## Author

Rhys Luffman, International School Manila, class of 2027.

## License

MIT

## Contact

Partnerships: [partner email]
Technical questions: [tech email]
