// SPDX-License-Identifier: Apache-2.0
import { copyFile, mkdir, stat } from "node:fs/promises";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";

const webRoot = fileURLToPath(new URL("..", import.meta.url));
const defaultSource = resolve(
  webRoot,
  "..",
  "..",
  "build",
  "wasm-release",
  "platforms",
  "web",
  "emscripten",
  "mol_audio_worklet_core.js",
);
const source = resolve(process.env.MOL_WORKLET_SOURCE ?? defaultSource);
const wasmSource = source.replace(/\.js$/u, ".wasm");
const destinationDirectory = resolve(webRoot, "public", "generated");
const destination = resolve(destinationDirectory, "mol_audio_worklet_core.js");
const wasmDestination = resolve(destinationDirectory, "mol_audio_worklet_core.wasm");

for (const artifact of [source, wasmSource]) {
  try {
    const details = await stat(artifact);
    if (!details.isFile()) throw new Error("source is not a regular file");
  } catch (error) {
    throw new Error(
      `Missing AudioWorklet build at ${artifact}. Build the wasm-release CMake preset first.`,
      { cause: error },
    );
  }
}

await mkdir(destinationDirectory, { recursive: true });
await copyFile(source, destination);
await copyFile(wasmSource, wasmDestination);
process.stdout.write(`Staged ${source} and ${wasmSource} -> ${destinationDirectory}\n`);
