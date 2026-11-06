declare namespace Cloudflare {
  interface Env {
    DB?: D1Database;
    NODE_1_WRITE_KEY?: string;
    NODE_2_WRITE_KEY?: string;
    NODE_3_WRITE_KEY?: string;
    BUCKET?: R2Bucket;
  }
}
