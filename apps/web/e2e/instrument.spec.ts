import { expect, test } from "@playwright/test";

test.beforeEach(async ({ page }) => {
  await page.goto("/");
});

test("renders the complete accessible instrument", async ({ page }) => {
  await expect(page).toHaveTitle("MoL Keyboard · Play the browser");
  await expect(page.getByRole("application")).toHaveCount(1);
  await expect(page.locator("[data-accessible-keys] button")).toHaveCount(30);
  await expect(page.getByRole("combobox", { name: "Instrument" })).toHaveCount(1);
  await expect(page.getByRole("button", { name: "Record" })).toHaveCount(1);
  await expect(page.locator('link[rel="manifest"]')).toHaveAttribute(
    "href",
    "./manifest.webmanifest",
  );
});

test("AudioWorklet survives a busy main thread and blur releases every note", async ({ page }, testInfo) => {
  test.skip(
    testInfo.project.name.includes("webkit"),
    "Playwright's Windows WebKit port does not expose AudioWorklet; real Safari is a separate device gate",
  );
  const pageErrors: string[] = [];
  page.on("pageerror", (error) => pageErrors.push(error.message));
  await page.getByRole("button", { name: "Start audio" }).click();
  await expect(page.locator("[data-status]")).toContainText("Audio ready");
  if (testInfo.project.name.startsWith("chrom") || testInfo.project.name === "edge-desktop") {
    await expect(page.locator("[data-fast-path]")).toHaveText("SharedArrayBuffer SPSC");
  }

  await page.keyboard.down("z");
  await expect(page.locator("[data-active-voices]")).not.toHaveText("0");
  const eventsBeforeStall = Number(await page.locator("[data-event-count]").textContent());
  await page.evaluate(() => {
    const deadline = performance.now() + 350;
    while (performance.now() < deadline) {
      Math.sqrt(987_654_321);
    }
  });
  await page.evaluate(() => window.dispatchEvent(new Event("blur")));
  await page.keyboard.up("z");
  await expect(page.locator("[data-active-voices]")).toHaveText("0");
  await expect
    .poll(async () => Number(await page.locator("[data-event-count]").textContent()))
    .toBeGreaterThan(eventsBeforeStall);
  await expect(page.locator("[data-dropped-count]")).toHaveText("0");
  expect(pageErrors).toEqual([]);
});

test("persists settings and installs an offline app shell", async ({ page, context }, testInfo) => {
  test.skip(testInfo.project.name !== "chrome-desktop", "One PWA lifecycle covers the shared bundle");
  await page.getByRole("combobox", { name: "Language" }).selectOption("zh");
  await page.getByRole("combobox", { name: "音色" }).selectOption("7");
  await page.waitForTimeout(250);
  await page.reload();
  await expect(page.getByRole("combobox", { name: "Language" })).toHaveValue("zh");
  await expect(page.getByRole("combobox", { name: "音色" })).toHaveValue("7");
  await page.evaluate(() => navigator.serviceWorker.ready);
  await context.setOffline(true);
  await page.reload({ waitUntil: "domcontentloaded" });
  await expect(page).toHaveTitle("MoL Keyboard · Play the browser");
  await expect(page.locator("[data-accessible-keys] button")).toHaveCount(30);
  await context.setOffline(false);
});

test("portrait mobile keeps playable key dimensions through horizontal scrolling", async ({ page }, testInfo) => {
  test.skip(!testInfo.project.name.includes("mobile"), "Mobile viewport contract");
  const dimensions = await page.locator("[data-keyboard-scroll]").evaluate((element) => ({
    clientWidth: element.clientWidth,
    scrollWidth: element.scrollWidth,
  }));
  expect(dimensions.scrollWidth).toBeGreaterThan(dimensions.clientWidth);
  if (testInfo.project.name === "webkit-mobile") return;
  await page.getByRole("button", { name: "Start audio" }).click();
  const before = Number(await page.locator("[data-event-count]").textContent());
  await page.getByRole("application").tap({ position: { x: 80, y: 160 } });
  await expect
    .poll(async () => Number(await page.locator("[data-event-count]").textContent()))
    .toBeGreaterThan(before);
});

test("non-isolated hosting uses the MessagePort baseline", async ({ page }, testInfo) => {
  test.skip(testInfo.project.name !== "chrome-desktop", "One engine validates the fallback transport");
  await page.goto("http://127.0.0.1:4175/");
  expect(await page.evaluate(() => window.crossOriginIsolated)).toBe(false);
  await page.getByRole("button", { name: "Start audio" }).click();
  await expect(page.locator("[data-fast-path]")).toHaveText("MessagePort baseline");
  const before = Number(await page.locator("[data-event-count]").textContent());
  await page.keyboard.press("z");
  await expect
    .poll(async () => Number(await page.locator("[data-event-count]").textContent()))
    .toBeGreaterThan(before);
});
