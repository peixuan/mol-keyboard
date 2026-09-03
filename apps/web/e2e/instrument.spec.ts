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

test("Firefox executes the Wasm DSP inside an offline AudioWorklet", async ({ page }, testInfo) => {
  test.skip(testInfo.project.name !== "firefox-desktop", "Firefox headless audio-device contract");
  const metrics = await page.evaluate(async () => {
    const context = new OfflineAudioContext(2, 48_000, 48_000);
    const wasmResponse = await fetch("./generated/mol_audio_worklet_core.wasm");
    if (!wasmResponse.ok) throw new Error("AudioWorklet Wasm unavailable");
    const wasmBinary = await wasmResponse.arrayBuffer();
    await context.audioWorklet.addModule("./generated/mol_audio_worklet_core.js");
    const node = new AudioWorkletNode(context, "mol-keyboard", {
      numberOfInputs: 0,
      numberOfOutputs: 1,
      outputChannelCount: [2],
      processorOptions: { initialNote: 60, wasmBinary },
    });
    let resolveReady: (message: unknown) => void = () => undefined;
    const ready = new Promise<unknown>((resolve) => {
      resolveReady = resolve;
    });
    let processorError = false;
    node.port.onmessage = (event) => {
      resolveReady(event.data);
    };
    node.addEventListener("processorerror", () => {
      processorError = true;
    });
    node.connect(context.destination);
    const readyMessage = await ready;
    const rendered = await context.startRendering();
    const left = rendered.getChannelData(0);
    let peak = 0;
    let finite = true;
    for (const sample of left) {
      peak = Math.max(peak, Math.abs(sample));
      finite &&= Number.isFinite(sample);
    }
    return { finite, peak, processorError, readyMessage };
  });
  expect(metrics.processorError).toBe(false);
  expect(metrics.readyMessage).toMatchObject({ error: undefined, ready: true, type: "ready" });
  expect(metrics.finite).toBe(true);
  expect(metrics.peak).toBeGreaterThan(0.01);
  expect(metrics.peak).toBeLessThanOrEqual(1);
});

test("AudioWorklet survives a busy main thread and blur releases every note", async ({ page }, testInfo) => {
  test.skip(
    testInfo.project.name.includes("webkit") || testInfo.project.name === "firefox-desktop",
    "This check needs a realtime audio device; Firefox DSP is covered with OfflineAudioContext",
  );
  const pageErrors: string[] = [];
  page.on("pageerror", (error) => pageErrors.push(error.message));
  await page.getByRole("button", { name: "Start audio" }).click();
  await expect(page.locator("[data-status]")).toContainText("Audio ready", { timeout: 10_000 });
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
  await page.evaluate(() => {
    Object.defineProperty(document, "visibilityState", { configurable: true, value: "hidden" });
    document.dispatchEvent(new Event("visibilitychange"));
  });
  await expect(page.locator("[data-status]")).toContainText("Audio suspended");
  await page.evaluate(() => {
    Object.defineProperty(document, "visibilityState", { configurable: true, value: "visible" });
    document.dispatchEvent(new Event("visibilitychange"));
  });
  await page.getByRole("button", { name: "Resume audio" }).click();
  await expect(page.locator("[data-status]")).toContainText("Audio ready");
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
  await page.getByRole("button", { name: "Start audio" }).click();
  await expect(page.locator("[data-status]")).toContainText("Audio ready", { timeout: 10_000 });
  const eventsBeforeOfflineNote = Number(await page.locator("[data-event-count]").textContent());
  await page.keyboard.press("z");
  await expect
    .poll(async () => Number(await page.locator("[data-event-count]").textContent()))
    .toBeGreaterThan(eventsBeforeOfflineNote);
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
