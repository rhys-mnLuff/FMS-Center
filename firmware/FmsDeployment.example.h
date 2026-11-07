#pragma once
// Copy to FmsDeployment.h, which is gitignored, and fill in the real values.
// Nothing in this file may be a real secret or a real phone number.

// Ingest endpoint. As of 7 Nov 2026 the base uploads to the Cloudflare
// Worker that owns storage, not to the hosted dashboard -- the dashboard
// reads from the same Worker and no longer stores anything itself.
#define FMS_USE_HTTPS 1
#define FMS_SERVER_HOST "fms-ingest.dashboard-data.workers.dev"

// workers.dev certificates chain to Google Trust Services Root R4, which is
// why firmware/certs/gtsr4.pem is the CA compiled in. If the endpoint moves
// to a host with a different issuer, that cert has to change with it.

#define FMS_SMS_ENABLED 0
#define FMS_API_KEY_NODE_1 "REPLACE_WITH_A_NEW_RANDOM_SECRET"

// Admin numbers the base texts directly, 2-4 of them, E.164 with no spaces.
// The 700-person lists are per-zone in the dashboard database, not here.
#define FMS_ADMIN_PHONES { "+639XXXXXXXXX", "+639YYYYYYYYY" }
