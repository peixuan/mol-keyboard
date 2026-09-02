import { defineConfig, devices } from "@playwright/test";

export default defineConfig({
  testDir: "./e2e",
  outputDir: "../../.cache/playwright-results",
  fullyParallel: false,
  workers: 1,
  timeout: 20_000,
  expect: { timeout: 5_000 },
  reporter: [["line"]],
  use: {
    baseURL: "http://127.0.0.1:4174",
    trace: "retain-on-failure",
    screenshot: "only-on-failure",
  },
  webServer: [
    {
      command: "npm run preview -- --port 4174 --strictPort",
      url: "http://127.0.0.1:4174",
      reuseExistingServer: false,
      timeout: 20_000,
    },
    {
      command: "node scripts/serve-static.mjs --port 4175",
      url: "http://127.0.0.1:4175",
      reuseExistingServer: false,
      timeout: 20_000,
    },
  ],
  projects: [
    {
      name: "chrome-desktop",
      use: { ...devices["Desktop Chrome"], channel: "chrome" },
    },
    {
      name: "edge-desktop",
      use: { ...devices["Desktop Edge"], channel: "msedge" },
    },
    {
      name: "firefox-desktop",
      use: { ...devices["Desktop Firefox"] },
    },
    {
      name: "webkit-desktop",
      use: { ...devices["Desktop Safari"] },
    },
    {
      name: "chromium-mobile",
      use: { ...devices["Pixel 7"] },
    },
    {
      name: "webkit-mobile",
      use: { ...devices["iPhone 14"] },
    },
  ],
});
