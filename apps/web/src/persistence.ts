const DATABASE_NAME = "mol-keyboard";
const DATABASE_VERSION = 1;
const SETTINGS_STORE = "settings";
const RECORDINGS_STORE = "recordings";
const MAX_RECORDING_BYTES = 8 * 1024 * 1024;

export interface WebSettings {
  readonly version: 1;
  readonly language: "en" | "zh";
  readonly mode: "explore" | "studio";
  readonly backend: "standalone" | "service" | "esp32" | "native";
  readonly preset: number;
  readonly scale: number;
  readonly tonic: number;
  readonly octave: number;
  readonly volume: number;
  readonly metronome: boolean;
  readonly chord: number;
  readonly arpeggiator: number;
  readonly arpeggiatorRate: number;
  readonly arpeggiatorGate: number;
  readonly arpeggiatorOctaves: number;
  readonly tempo: number;
  readonly timeSignature: "3/4" | "4/4" | "5/4" | "6/8";
  readonly portamento: number;
  readonly portamentoTime: number;
  readonly chorus: number;
  readonly delay: number;
  readonly reverb: number;
}

export interface SettingsLoadResult {
  readonly settings: WebSettings;
  readonly storage: "indexeddb" | "unavailable";
  readonly diagnostic?: string;
}

export interface RecordingMetadata {
  readonly id: string;
  readonly name: string;
  readonly createdAt: string;
  readonly size: number;
  readonly storage: "indexeddb" | "opfs";
}

interface StoredRecording extends RecordingMetadata {
  readonly bytes?: ArrayBuffer;
  readonly fileName?: string;
}

export const DEFAULT_SETTINGS: WebSettings = {
  version: 1,
  language: navigator.language.toLowerCase().startsWith("zh") ? "zh" : "en",
  mode: "explore",
  backend: "standalone",
  preset: 0,
  scale: 0,
  tonic: 0,
  octave: 0,
  volume: 0.72,
  metronome: false,
  chord: 0,
  arpeggiator: 0,
  arpeggiatorRate: 3,
  arpeggiatorGate: 0.7,
  arpeggiatorOctaves: 2,
  tempo: 120,
  timeSignature: "4/4",
  portamento: 0,
  portamentoTime: 120,
  chorus: 0.18,
  delay: 0.12,
  reverb: 0.22,
};

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function integerInRange(value: unknown, minimum: number, maximum: number): value is number {
  return Number.isInteger(value) && (value as number) >= minimum && (value as number) <= maximum;
}

function numberInRange(value: unknown, minimum: number, maximum: number): value is number {
  return typeof value === "number" && Number.isFinite(value) && value >= minimum && value <= maximum;
}

export function validateSettings(value: unknown): value is WebSettings {
  if (!isRecord(value)) return false;
  const expectedKeys = Object.keys(DEFAULT_SETTINGS).sort();
  const actualKeys = Object.keys(value).sort();
  if (actualKeys.length !== expectedKeys.length || actualKeys.some((key, index) => key !== expectedKeys[index])) {
    return false;
  }
  return (
    value.version === 1 &&
    (value.language === "en" || value.language === "zh") &&
    (value.mode === "explore" || value.mode === "studio") &&
    (value.backend === "standalone" ||
      value.backend === "service" ||
      value.backend === "esp32" ||
      value.backend === "native") &&
    integerInRange(value.preset, 0, 17) &&
    integerInRange(value.scale, 0, 7) &&
    integerInRange(value.tonic, 0, 11) &&
    integerInRange(value.octave, -2, 2) &&
    numberInRange(value.volume, 0, 1) &&
    typeof value.metronome === "boolean" &&
    integerInRange(value.chord, 0, 9) &&
    integerInRange(value.arpeggiator, 0, 6) &&
    integerInRange(value.arpeggiatorRate, 0, 5) &&
    numberInRange(value.arpeggiatorGate, 0.05, 1) &&
    integerInRange(value.arpeggiatorOctaves, 1, 4) &&
    numberInRange(value.tempo, 30, 300) &&
    ["3/4", "4/4", "5/4", "6/8"].includes(value.timeSignature as string) &&
    integerInRange(value.portamento, 0, 2) &&
    numberInRange(value.portamentoTime, 0, 2000) &&
    numberInRange(value.chorus, 0, 1) &&
    numberInRange(value.delay, 0, 1) &&
    numberInRange(value.reverb, 0, 1)
  );
}

function requestResult<T>(request: IDBRequest<T>): Promise<T> {
  return new Promise((resolve, reject) => {
    request.addEventListener("success", () => resolve(request.result), { once: true });
    request.addEventListener("error", () => reject(request.error ?? new Error("IndexedDB request failed")), {
      once: true,
    });
  });
}

function transactionComplete(transaction: IDBTransaction): Promise<void> {
  return new Promise((resolve, reject) => {
    transaction.addEventListener("complete", () => resolve(), { once: true });
    transaction.addEventListener(
      "abort",
      () => reject(transaction.error ?? new Error("IndexedDB transaction aborted")),
      { once: true },
    );
    transaction.addEventListener(
      "error",
      () => reject(transaction.error ?? new Error("IndexedDB transaction failed")),
      { once: true },
    );
  });
}

function openDatabase(): Promise<IDBDatabase> {
  if (!("indexedDB" in window)) return Promise.reject(new Error("IndexedDB is unavailable"));
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(DATABASE_NAME, DATABASE_VERSION);
    request.addEventListener("upgradeneeded", () => {
      const database = request.result;
      if (!database.objectStoreNames.contains(SETTINGS_STORE)) {
        database.createObjectStore(SETTINGS_STORE, { keyPath: "key" });
      }
      if (!database.objectStoreNames.contains(RECORDINGS_STORE)) {
        database.createObjectStore(RECORDINGS_STORE, { keyPath: "id" });
      }
    });
    request.addEventListener("success", () => resolve(request.result), { once: true });
    request.addEventListener("error", () => reject(request.error ?? new Error("Could not open IndexedDB")), {
      once: true,
    });
    request.addEventListener("blocked", () => reject(new Error("IndexedDB upgrade was blocked")), {
      once: true,
    });
  });
}

export async function loadSettings(): Promise<SettingsLoadResult> {
  let database: IDBDatabase | undefined;
  try {
    database = await openDatabase();
    const transaction = database.transaction(SETTINGS_STORE, "readonly");
    const record = (await requestResult(
      transaction.objectStore(SETTINGS_STORE).get("current"),
    )) as { readonly value?: unknown } | undefined;
    await transactionComplete(transaction);
    if (record === undefined) return { settings: DEFAULT_SETTINGS, storage: "indexeddb" };
    if (!validateSettings(record.value)) {
      return {
        settings: DEFAULT_SETTINGS,
        storage: "indexeddb",
        diagnostic: "Stored settings failed schema validation; defaults were restored.",
      };
    }
    return { settings: record.value, storage: "indexeddb" };
  } catch (error: unknown) {
    return {
      settings: DEFAULT_SETTINGS,
      storage: "unavailable",
      diagnostic: error instanceof Error ? error.message : "Persistent storage failed.",
    };
  } finally {
    database?.close();
  }
}

export async function saveSettings(settings: WebSettings): Promise<void> {
  if (!validateSettings(settings)) throw new Error("Refusing to save invalid Web settings");
  const database = await openDatabase();
  try {
    const transaction = database.transaction(SETTINGS_STORE, "readwrite");
    transaction.objectStore(SETTINGS_STORE).put({ key: "current", value: settings });
    await transactionComplete(transaction);
  } finally {
    database.close();
  }
}

async function saveToOpfs(id: string, bytes: Uint8Array): Promise<string | undefined> {
  if (!("storage" in navigator) || typeof navigator.storage.getDirectory !== "function") return undefined;
  const root = await navigator.storage.getDirectory();
  const directory = await root.getDirectoryHandle("recordings", { create: true });
  const temporaryName = `${id}.tmp`;
  const finalName = `${id}.molseq`;
  const temporary = await directory.getFileHandle(temporaryName, { create: true });
  const writable = await temporary.createWritable();
  try {
    const copy = new Uint8Array(new ArrayBuffer(bytes.length));
    copy.set(bytes);
    await writable.write(copy);
    await writable.close();
  } catch (error: unknown) {
    await writable.abort();
    await directory.removeEntry(temporaryName).catch(() => undefined);
    throw error;
  }
  const movable = temporary as FileSystemFileHandle & { move?: (name: string) => Promise<void> };
  if (typeof movable.move !== "function") {
    await directory.removeEntry(temporaryName).catch(() => undefined);
    return undefined;
  }
  await movable.move(finalName);
  return finalName;
}

export async function saveRecording(bytes: Uint8Array, name: string): Promise<RecordingMetadata> {
  if (bytes.length === 0 || bytes.length > MAX_RECORDING_BYTES) {
    throw new Error(`Recording must contain 1 to ${MAX_RECORDING_BYTES} bytes`);
  }
  const id = crypto.randomUUID();
  const safeName = name.trim().slice(0, 80) || "Untitled take";
  let fileName: string | undefined;
  try {
    fileName = await saveToOpfs(id, bytes);
  } catch {
    fileName = undefined;
  }
  const metadata: RecordingMetadata = {
    id,
    name: safeName,
    createdAt: new Date().toISOString(),
    size: bytes.length,
    storage: fileName === undefined ? "indexeddb" : "opfs",
  };
  const stored: StoredRecording =
    fileName === undefined
      ? { ...metadata, bytes: bytes.slice().buffer }
      : { ...metadata, fileName };
  const database = await openDatabase();
  try {
    const transaction = database.transaction(RECORDINGS_STORE, "readwrite");
    transaction.objectStore(RECORDINGS_STORE).put(stored);
    await transactionComplete(transaction);
  } finally {
    database.close();
  }
  return metadata;
}

export async function listRecordings(): Promise<readonly RecordingMetadata[]> {
  const database = await openDatabase();
  try {
    const transaction = database.transaction(RECORDINGS_STORE, "readonly");
    const records = (await requestResult(
      transaction.objectStore(RECORDINGS_STORE).getAll(),
    )) as StoredRecording[];
    await transactionComplete(transaction);
    return records
      .map(({ id, name, createdAt, size, storage }) => ({ id, name, createdAt, size, storage }))
      .sort((left, right) => right.createdAt.localeCompare(left.createdAt));
  } finally {
    database.close();
  }
}

export async function loadRecording(id: string): Promise<Uint8Array> {
  const database = await openDatabase();
  let record: StoredRecording | undefined;
  try {
    const transaction = database.transaction(RECORDINGS_STORE, "readonly");
    record = (await requestResult(transaction.objectStore(RECORDINGS_STORE).get(id))) as
      | StoredRecording
      | undefined;
    await transactionComplete(transaction);
  } finally {
    database.close();
  }
  if (record === undefined) throw new Error("Recording not found");
  if (record.storage === "indexeddb" && record.bytes !== undefined) return new Uint8Array(record.bytes);
  if (record.fileName === undefined || typeof navigator.storage.getDirectory !== "function") {
    throw new Error("The OPFS recording is unavailable");
  }
  const root = await navigator.storage.getDirectory();
  const directory = await root.getDirectoryHandle("recordings");
  const handle = await directory.getFileHandle(record.fileName);
  const file = await handle.getFile();
  if (file.size === 0 || file.size > MAX_RECORDING_BYTES) throw new Error("Invalid recording size");
  return new Uint8Array(await file.arrayBuffer());
}
