import { defineConfig, devices } from '@playwright/test';

const baseURL = process.env.PLAYWRIGHT_BASE_URL || 'http://127.0.0.1:8080';

export default defineConfig({
  testDir: './tests',
  fullyParallel: true,
  maxFailures: process.env.CI ? 1 : undefined,
  forbidOnly: !!process.env.CI,
  retries: 0,
  workers: 1,
  reporter: [
    ['list'], ['html', { open: 'never' }], ['./reporters/visual-comparison-reporter.js']
  ],
  use: {
    baseURL,
    trace: 'on',
    screenshot: { mode: 'on', fullPage: true },
    video: 'on',
    viewport: { width: 1920, height: 1080 },
  },
  projects: [
    // ── UI projects ──────────────────────────────────────────────────────────
    {
      name: 'chromium-light',
      testMatch: ['**/fpp.spec.ts', '**/theme-override.spec.ts'],
      use: { ...devices['Desktop Chrome'], colorScheme: 'light' },
    },
    {
      name: 'chromium-dark',
      testMatch: ['**/fpp.spec.ts', '**/theme-override.spec.ts'],
      use: { ...devices['Desktop Chrome'], colorScheme: 'dark' },
    },

    // ── API v2 projects ───────────────────────────────────────────────────────
    // CI: FULL + SCHEMA only — no @state, @hardware:*, or @destructive tags
    // Run via: npm run test:api:ci
    {
      name: 'api-ci',
      testMatch: '**/api-v2/**/*.spec.ts',
      use: { baseURL },
    },

    // Stateful tests — require a writable FPP instance; manage their own setup/teardown
    // Run via: npm run test:api:state
    {
      name: 'api-state',
      testMatch: '**/api-v2/**/*.spec.ts',
      use: { baseURL },
    },

    // Hardware-gated projects — one per capability; point at real hardware via PLAYWRIGHT_BASE_URL
    // Run via: npm run test:api:hardware:cape  (or :wifi, :audio, :pipewire)
    {
      name: 'api-hardware-cape',
      testMatch: '**/api-v2/**/*.spec.ts',
      use: { baseURL },
    },
    {
      name: 'api-hardware-wifi',
      testMatch: '**/api-v2/**/*.spec.ts',
      use: { baseURL },
    },
    {
      name: 'api-hardware-audio',
      testMatch: '**/api-v2/**/*.spec.ts',
      use: { baseURL },
    },
    {
      name: 'api-hardware-pipewire',
      testMatch: '**/api-v2/**/*.spec.ts',
      use: { baseURL },
    },

    // Destructive — kills fppd / reboots / shuts down; run only on sacrificial hardware
    // Run via: npm run test:api:destructive
    {
      name: 'api-destructive',
      testMatch: '**/api-v2/**/*.spec.ts',
      use: { baseURL },
    },
  ],
  outputDir: 'test-results',
});
