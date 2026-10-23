import { defineConfig } from '@playwright/test';

export default defineConfig({
  testDir: '.',
  testMatch: '**/*.spec.mjs',
  reporter: 'list',
  use: { browserName: 'chromium', serviceWorkers: 'block', offline: true },
  workers: 1,
});
