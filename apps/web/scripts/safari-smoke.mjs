// SPDX-License-Identifier: Apache-2.0

import fs from "node:fs/promises";
import path from "node:path";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";

const WEBDRIVER_ELEMENT_KEY = "element-6066-11e4-a52e-4f735466cecf";
const DRIVER_PORT = 4444;
const WEB_PORT = 4176;
const DRIVER_URL = `http://127.0.0.1:${DRIVER_PORT}`;
const WEB_URL = `http://127.0.0.1:${WEB_PORT}`;
const directory = path.dirname(fileURLToPath(import.meta.url));
const application = path.resolve(directory, "..");
const serverScript = path.join(directory, "serve-static.mjs");
const builtIndex = path.join(application, "dist", "index.html");
const children = [];
let sessionId;

function delay(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

function launch(command, arguments_, name) {
  const child = spawn(command, arguments_, {
    cwd: application,
    env: process.env,
    stdio: ["ignore", "pipe", "pipe"],
  });
  const output = [];
  for (const stream of [child.stdout, child.stderr]) {
    stream.setEncoding("utf8");
    stream.on("data", (chunk) => {
      output.push(chunk);
      if (output.length > 100) output.shift();
    });
  }
  child.on("error", (error) => output.push(`${error.message}\n`));
  children.push({ child, name, output });
  return child;
}

async function waitForUrl(url, name) {
  for (let attempt = 0; attempt < 100; attempt += 1) {
    try {
      const response = await fetch(url, { signal: AbortSignal.timeout(1_000) });
      if (response.ok) return;
    } catch {
      // The process can take several seconds to bind on a fresh runner.
    }
    await delay(100);
  }
  throw new Error(`${name} did not become ready at ${url}`);
}

async function webdriver(method, endpoint, payload) {
  const response = await fetch(`${DRIVER_URL}${endpoint}`, {
    method,
    headers: payload === undefined ? undefined : { "Content-Type": "application/json" },
    body: payload === undefined ? undefined : JSON.stringify(payload),
    signal: AbortSignal.timeout(20_000),
  });
  const body = await response.text();
  const decoded = body.length === 0 ? { value: null } : JSON.parse(body);
  if (!response.ok || decoded.value?.error !== undefined) {
    const message = decoded.value?.message ?? `${response.status} ${response.statusText}`;
    throw new Error(`SafariDriver ${method} ${endpoint} failed: ${message}`);
  }
  return decoded.value;
}

async function execute(script) {
  return webdriver("POST", `/session/${sessionId}/execute/sync`, { script, args: [] });
}

async function waitForScript(script, description) {
  for (let attempt = 0; attempt < 100; attempt += 1) {
    if (await execute(script)) return;
    await delay(100);
  }
  throw new Error(`Timed out waiting for ${description}`);
}

async function stopChild(entry) {
  if (entry.child.exitCode !== null || entry.child.signalCode !== null) return;
  entry.child.kill("SIGTERM");
  await Promise.race([
    new Promise((resolve) => entry.child.once("exit", resolve)),
    delay(2_000),
  ]);
  if (entry.child.exitCode === null && entry.child.signalCode === null) entry.child.kill("SIGKILL");
}

async function cleanup() {
  if (sessionId !== undefined) {
    try {
      await webdriver("DELETE", `/session/${sessionId}`);
    } catch (error) {
      process.stderr.write(`Could not close SafariDriver session: ${error.message}\n`);
    }
    sessionId = undefined;
  }
  for (const entry of [...children].reverse()) await stopChild(entry);
}

async function main() {
  if (process.platform !== "darwin") throw new Error("The Safari smoke requires macOS");
  await fs.access(builtIndex);

  launch(process.execPath, [serverScript, "--port", String(WEB_PORT)], "static server");
  launch(process.env.MOL_SAFARIDRIVER ?? "safaridriver", ["--port", String(DRIVER_PORT)], "SafariDriver");
  await Promise.all([
    waitForUrl(`${WEB_URL}/`, "production Web application"),
    waitForUrl(`${DRIVER_URL}/status`, "SafariDriver"),
  ]);

  const session = await webdriver("POST", "/session", {
    capabilities: { alwaysMatch: { browserName: "safari" } },
  });
  sessionId = session.sessionId;
  if (typeof sessionId !== "string" || sessionId.length === 0) {
    throw new Error("SafariDriver returned no session identifier");
  }
  if (String(session.capabilities?.browserName).toLowerCase() !== "safari") {
    throw new Error(`Expected Safari, got ${session.capabilities?.browserName ?? "unknown browser"}`);
  }
  if (typeof session.capabilities?.browserVersion !== "string") {
    throw new Error("SafariDriver returned no browser version");
  }

  await webdriver("POST", `/session/${sessionId}/url`, { url: `${WEB_URL}/` });
  await waitForScript(
    "return document.readyState === 'complete' && document.querySelectorAll('[data-accessible-keys] button').length === 30;",
    "the complete instrument UI",
  );
  const initial = await execute(`
    return {
      title: document.title,
      applicationCount: document.querySelectorAll('[role="application"]').length,
      keyCount: document.querySelectorAll('[data-accessible-keys] button').length,
      manifest: document.querySelector('link[rel="manifest"]')?.getAttribute('href'),
      audioContext: typeof AudioContext === 'function',
      audioWorkletNode: typeof AudioWorkletNode === 'function',
      serviceWorker: 'serviceWorker' in navigator,
      indexedDb: typeof indexedDB === 'object',
      userAgent: navigator.userAgent,
    };
  `);
  if (
    initial.title !== "MoL Keyboard · Play the browser" ||
    initial.applicationCount !== 1 ||
    initial.keyCount !== 30 ||
    initial.manifest !== "./manifest.webmanifest" ||
    !initial.audioContext ||
    !initial.audioWorkletNode ||
    !initial.serviceWorker ||
    !initial.indexedDb ||
    !/Version\/.+ Safari\//.test(initial.userAgent)
  ) {
    throw new Error(`Safari capability/UI contract failed: ${JSON.stringify(initial)}`);
  }

  const start = await webdriver("POST", `/session/${sessionId}/element`, {
    using: "css selector",
    value: "[data-action='start']",
  });
  const startId = start[WEBDRIVER_ELEMENT_KEY];
  if (typeof startId !== "string") throw new Error("SafariDriver could not locate the audio start control");
  await webdriver("POST", `/session/${sessionId}/element/${startId}/click`);
  await waitForScript(
    "return document.querySelector('[data-status]')?.textContent?.startsWith('Audio ready') === true;",
    "Safari AudioWorklet/Wasm startup",
  );

  const before = Number(await execute("return document.querySelector('[data-event-count]')?.textContent;"));
  const body = await webdriver("POST", `/session/${sessionId}/element`, {
    using: "css selector",
    value: "body",
  });
  const bodyId = body[WEBDRIVER_ELEMENT_KEY];
  if (typeof bodyId !== "string") throw new Error("SafariDriver could not locate the document body");
  await webdriver("POST", `/session/${sessionId}/element/${bodyId}/value`, { text: "z", value: ["z"] });
  await waitForScript(
    `return Number(document.querySelector('[data-event-count]')?.textContent) > ${before};`,
    "a Safari keyboard event to reach the Wasm engine",
  );
  const finalState = await execute(`
    return {
      status: document.querySelector('[data-status]')?.textContent,
      fastPath: document.querySelector('[data-fast-path]')?.textContent,
      eventCount: Number(document.querySelector('[data-event-count]')?.textContent),
      droppedCount: Number(document.querySelector('[data-dropped-count]')?.textContent),
      activeVoices: Number(document.querySelector('[data-active-voices]')?.textContent),
    };
  `);
  if (
    !finalState.status?.startsWith("Audio ready") ||
    finalState.fastPath !== "MessagePort baseline" ||
    finalState.eventCount <= before ||
    finalState.droppedCount !== 0 ||
    finalState.activeVoices !== 0
  ) {
    throw new Error(`Safari runtime contract failed: ${JSON.stringify(finalState)}`);
  }

  process.stdout.write(
    `MOL_SAFARI_SMOKE_PASS ${session.capabilities.browserVersion} ` +
      `events=${finalState.eventCount} transport=${finalState.fastPath}\n`,
  );
}

try {
  await main();
} catch (error) {
  for (const entry of children) {
    if (entry.output.length > 0) {
      process.stderr.write(`--- ${entry.name} output ---\n${entry.output.join("")}`);
    }
  }
  throw error;
} finally {
  await cleanup();
}
