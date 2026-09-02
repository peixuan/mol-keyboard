import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { access, mkdtemp, rm } from "node:fs/promises";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import { expect, test } from "@playwright/test";

interface ServiceStartup {
  readonly endpoint: string;
  readonly token: string;
}

function waitForService(child: ChildProcessWithoutNullStreams): Promise<ServiceStartup> {
  return new Promise((resolve, reject) => {
    let output = "";
    let diagnostics = "";
    const timer = setTimeout(() => reject(new Error(`Service startup timed out: ${diagnostics}`)), 8_000);
    child.stderr.on("data", (chunk: Buffer) => {
      diagnostics = (diagnostics + chunk.toString("utf8")).slice(-2_048);
    });
    child.stdout.on("data", (chunk: Buffer) => {
      output += chunk.toString("utf8");
      const endpoint = output.match(/websocket=(ws:\/\/127\.0\.0\.1:\d+\/control)/u)?.[1];
      const token = output.match(/token=([0-9a-f]{64})/u)?.[1];
      if (endpoint === undefined || token === undefined) return;
      clearTimeout(timer);
      resolve({ endpoint, token });
    });
    child.once("exit", (code) => {
      clearTimeout(timer);
      reject(new Error(`Service exited during startup with ${String(code)}: ${diagnostics}`));
    });
  });
}

function shutdownService(endpoint: string): Promise<void> {
  return new Promise((resolve, reject) => {
    const request = Buffer.from(
      JSON.stringify({ jsonrpc: "2.0", method: "system.shutdown", params: {}, id: 1 }),
      "utf8",
    );
    const frame = Buffer.allocUnsafe(request.length + 4);
    frame.writeUInt32LE(request.length, 0);
    request.copy(frame, 4);
    const socket = net.createConnection(endpoint);
    let response = Buffer.alloc(0);
    socket.setTimeout(2_000);
    socket.on("connect", () => socket.write(frame));
    socket.on("data", (chunk) => {
      response = Buffer.concat([response, chunk]);
      if (response.length < 4 || response.length < response.readUInt32LE(0) + 4) return;
      socket.end();
      resolve();
    });
    socket.on("timeout", () => socket.destroy(new Error("Local IPC shutdown timed out")));
    socket.on("error", reject);
  });
}

test("controls a real authenticated desktop service", async ({ page }, testInfo) => {
  test.skip(testInfo.project.name !== "chrome-desktop", "One stable browser covers service IPC");
  const repository = path.resolve(process.cwd(), "..", "..");
  const defaultExecutable =
    process.platform === "win32"
      ? path.join(
          repository,
          "build",
          "ci-windows-msvc",
          "apps",
          "mol-keyboardd",
          "mol-keyboardd.exe",
        )
      : path.join(repository, "build", "ci-linux-clang", "apps", "mol-keyboardd", "mol-keyboardd");
  const executable = process.env.MOL_DAEMON ?? defaultExecutable;
  const executableExists = await access(executable).then(
    () => true,
    () => false,
  );
  test.skip(!executableExists, `Build mol-keyboardd first or set MOL_DAEMON (looked for ${executable})`);

  const state = await mkdtemp(path.join(os.tmpdir(), "mol-web-service-e2e-"));
  const localEndpoint =
    process.platform === "win32"
      ? `\\\\.\\pipe\\mol-keyboard-web-e2e-${process.pid}-${Date.now()}`
      : path.join(state, "control.sock");
  const child = spawn(
    executable,
    [
      "--null-backend",
      "--state-dir",
      state,
      "--endpoint",
      localEndpoint,
      "--websocket-port",
      "0",
      "--web-origin",
      "http://127.0.0.1:4174",
    ],
    { stdio: ["pipe", "pipe", "pipe"] },
  );

  try {
    const service = await waitForService(child);
    await page.goto("/");
    await page.getByRole("combobox", { name: "Audio backend" }).selectOption("service");
    await page.getByLabel("Endpoint").fill(service.endpoint);
    await page.getByLabel("Session token").fill(service.token);
    await page.getByRole("button", { name: "Connect", exact: true }).click();
    await expect(page.locator("[data-service-status]")).toContainText("Connected");
    await expect(page.locator("[data-fast-path]")).toHaveText("Authenticated loopback WebSocket");
    await expect(page.locator("[data-service-token]")).toHaveValue("");

    const before = Number(await page.locator("[data-event-count]").textContent());
    await page.keyboard.press("z", { delay: 150 });
    await expect
      .poll(async () => Number(await page.locator("[data-event-count]").textContent()))
      .toBeGreaterThan(before);
    await expect(page.locator("[data-active-voices]")).toHaveText("0");

    await page.getByRole("button", { name: "Record", exact: true }).click();
    await expect(page.locator("[data-recording-state]")).toContainText("Recording");
    await page.keyboard.press("x", { delay: 120 });
    await page.getByRole("button", { name: "Stop", exact: true }).click();
    await expect(page.locator("[data-recording-list] option")).toHaveCount(2);
    await expect(page.locator("[data-recording-storage]")).toContainText("Desktop service");

    await page.getByRole("button", { name: "Disconnect", exact: true }).click();
    await page.getByLabel("Session token").fill("f".repeat(64));
    await page.getByRole("button", { name: "Connect", exact: true }).click();
    await expect(page.locator("[data-service-status]")).toContainText("rejected");
  } finally {
    await shutdownService(localEndpoint).catch(() => child.kill());
    await new Promise<void>((resolve) => {
      if (child.exitCode !== null) resolve();
      else child.once("exit", () => resolve());
    });
    const resolvedState = path.resolve(state);
    if (resolvedState.startsWith(path.resolve(os.tmpdir()) + path.sep)) {
      await rm(resolvedState, { recursive: true, force: true });
    }
  }
});
