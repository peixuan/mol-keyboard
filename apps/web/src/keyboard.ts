export interface KeyBinding {
  readonly code: string;
  readonly label: string;
  readonly note: number;
}

export interface KeyRectangle {
  readonly note: number;
  readonly x: number;
  readonly y: number;
  readonly width: number;
  readonly height: number;
  readonly black: boolean;
}

export const FIRST_NOTE = 60;
export const LAST_NOTE = 89;
export const KEYBOARD_HEIGHT = 236;
export const WHITE_KEY_WIDTH = 60;
export const BLACK_KEY_WIDTH = 38;
export const BLACK_KEY_HEIGHT = 142;

const PHYSICAL_CODES = [
  "KeyZ",
  "KeyS",
  "KeyX",
  "KeyD",
  "KeyC",
  "KeyV",
  "KeyG",
  "KeyB",
  "KeyH",
  "KeyN",
  "KeyJ",
  "KeyM",
  "KeyQ",
  "Digit2",
  "KeyW",
  "Digit3",
  "KeyE",
  "KeyR",
  "Digit5",
  "KeyT",
  "Digit6",
  "KeyY",
  "Digit7",
  "KeyU",
  "KeyI",
  "Digit9",
  "KeyO",
  "Digit0",
  "KeyP",
  "BracketLeft",
] as const;

const DISPLAY_LABELS = [
  "Z",
  "S",
  "X",
  "D",
  "C",
  "V",
  "G",
  "B",
  "H",
  "N",
  "J",
  "M",
  "Q",
  "2",
  "W",
  "3",
  "E",
  "R",
  "5",
  "T",
  "6",
  "Y",
  "7",
  "U",
  "I",
  "9",
  "O",
  "0",
  "P",
  "[",
] as const;

const NOTE_NAMES = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"];
const BLACK_CLASSES = new Set([1, 3, 6, 8, 10]);

export const KEY_BINDINGS: readonly KeyBinding[] = PHYSICAL_CODES.map((code, index) => ({
  code,
  label: DISPLAY_LABELS[index] ?? "",
  note: FIRST_NOTE + index,
}));

export const BINDING_BY_CODE = new Map(KEY_BINDINGS.map((binding) => [binding.code, binding]));
export const BINDING_BY_NOTE = new Map(KEY_BINDINGS.map((binding) => [binding.note, binding]));

export function isBlackNote(note: number): boolean {
  return BLACK_CLASSES.has(note % 12);
}

export function noteName(note: number): string {
  const name = NOTE_NAMES[note % 12] ?? "?";
  return `${name}${Math.floor(note / 12) - 1}`;
}

export function createKeyRectangles(): readonly KeyRectangle[] {
  const whiteKeys: KeyRectangle[] = [];
  const blackKeys: KeyRectangle[] = [];
  let whiteIndex = 0;
  for (let note = FIRST_NOTE; note <= LAST_NOTE; note += 1) {
    if (isBlackNote(note)) {
      blackKeys.push({
        note,
        x: whiteIndex * WHITE_KEY_WIDTH - BLACK_KEY_WIDTH / 2,
        y: 0,
        width: BLACK_KEY_WIDTH,
        height: BLACK_KEY_HEIGHT,
        black: true,
      });
    } else {
      whiteKeys.push({
        note,
        x: whiteIndex * WHITE_KEY_WIDTH,
        y: 0,
        width: WHITE_KEY_WIDTH,
        height: KEYBOARD_HEIGHT,
        black: false,
      });
      whiteIndex += 1;
    }
  }
  return [...whiteKeys, ...blackKeys];
}

export const KEY_RECTANGLES = createKeyRectangles();
export const KEYBOARD_WIDTH =
  KEY_RECTANGLES.filter((rectangle) => !rectangle.black).length * WHITE_KEY_WIDTH;

export function noteAtPoint(x: number, y: number): number | undefined {
  for (let index = KEY_RECTANGLES.length - 1; index >= 0; index -= 1) {
    const key = KEY_RECTANGLES[index];
    if (
      key !== undefined &&
      x >= key.x &&
      x < key.x + key.width &&
      y >= key.y &&
      y < key.y + key.height
    ) {
      return key.note;
    }
  }
  return undefined;
}
