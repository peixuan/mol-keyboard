import { spawnSync } from "node:child_process";
import path from "node:path";
import { fileURLToPath } from "node:url";

const directory = path.dirname(fileURLToPath(import.meta.url));
const application = path.resolve(directory, "..");
const repository = path.resolve(application, "..", "..");
const cli = path.join(application, "node_modules", "@playwright", "test", "cli.js");
const environment = {
  ...process.env,
  PLAYWRIGHT_BROWSERS_PATH:
    process.env.PLAYWRIGHT_BROWSERS_PATH ?? path.join(repository, ".cache", "playwright"),
};
const result = spawnSync(process.execPath, [cli, ...process.argv.slice(2)], {
  cwd: application,
  env: environment,
  stdio: "inherit",
});
if (result.error !== undefined) throw result.error;
process.exitCode = result.status ?? 1;
