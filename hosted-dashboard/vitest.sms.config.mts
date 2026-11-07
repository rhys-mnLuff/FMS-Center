// The project's vite.config.ts pulls in vinext, which imports `glob` from
// node:fs/promises -- added in Node 22, and this machine runs Node 20. These
// pure-logic tests need none of that, so they get a standalone config. No
// `defineConfig` import, because vitest is not installed locally.
export default { test: { include: ["lib/sms-*.test.ts", "lib/subscribers.test.ts"] } };
