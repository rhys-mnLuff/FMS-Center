declare namespace Cloudflare {
  interface Env {
    DB?: D1Database;
    NODE_1_WRITE_KEY?: string;
    NODE_2_WRITE_KEY?: string;
    NODE_3_WRITE_KEY?: string;
    BUCKET?: R2Bucket;
    SEMAPHORE_API_KEY?: string;    // secret
    SEMAPHORE_SENDER_NAME?: string; // optional; omitted = the account's default sender name
    ADMIN_NUMBERS?: string;        // secret, comma-separated; receives battery alerts
    SMS_OPT_OUT?: string;          // "1" to append "Reply STOP" -- only once /sms-inbound is wired
    SMS_INBOUND_KEY?: string;      // secret guarding /sms-inbound
    SMS_ADMIN_KEY?: string;        // secret guarding POST /sms-subscribers
    SITE_NAME_1?: string; SITE_NAME_2?: string; SITE_NAME_3?: string;
    SITE_LAT_LONG?: string;
  }
}
