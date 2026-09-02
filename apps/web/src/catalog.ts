export interface LocalizedOption {
  readonly value: number;
  readonly en: string;
  readonly zh: string;
}

export const PRESETS: readonly LocalizedOption[] = [
  { value: 0, en: "Grand Piano", zh: "大钢琴" },
  { value: 1, en: "Electric Piano", zh: "电钢琴" },
  { value: 2, en: "Harpsichord", zh: "大键琴" },
  { value: 3, en: "Church Organ", zh: "教堂风琴" },
  { value: 4, en: "Jazz Organ", zh: "爵士风琴" },
  { value: 5, en: "Nylon Guitar", zh: "尼龙吉他" },
  { value: 6, en: "Steel Guitar", zh: "钢弦吉他" },
  { value: 7, en: "Violin", zh: "小提琴" },
  { value: 8, en: "Cello", zh: "大提琴" },
  { value: 9, en: "Flute", zh: "长笛" },
  { value: 10, en: "Clarinet", zh: "单簧管" },
  { value: 11, en: "Synth Lead", zh: "合成器主音" },
  { value: 12, en: "Synth Pad", zh: "合成器铺底" },
  { value: 13, en: "Synth Bass", zh: "合成贝斯" },
  { value: 14, en: "Choir", zh: "合唱" },
  { value: 15, en: "Vibraphone", zh: "颤音琴" },
  { value: 16, en: "Harp", zh: "竖琴" },
  { value: 17, en: "Music Box", zh: "八音盒" },
];

export const SCALES: readonly LocalizedOption[] = [
  { value: 0, en: "Chromatic", zh: "半音阶" },
  { value: 1, en: "Major", zh: "大调" },
  { value: 2, en: "Natural minor", zh: "自然小调" },
  { value: 3, en: "Major pentatonic", zh: "大调五声音阶" },
  { value: 4, en: "Minor pentatonic", zh: "小调五声音阶" },
  { value: 5, en: "Blues", zh: "布鲁斯" },
  { value: 6, en: "Dorian", zh: "多利亚" },
  { value: 7, en: "Mixolydian", zh: "混合利底亚" },
];

export const CHORDS: readonly LocalizedOption[] = [
  { value: 0, en: "Off", zh: "关闭" },
  { value: 1, en: "Major", zh: "大三和弦" },
  { value: 2, en: "Minor", zh: "小三和弦" },
  { value: 3, en: "Suspended 2", zh: "挂二和弦" },
  { value: 4, en: "Suspended 4", zh: "挂四和弦" },
  { value: 5, en: "Dominant 7", zh: "属七和弦" },
  { value: 6, en: "Major 7", zh: "大七和弦" },
  { value: 7, en: "Minor 7", zh: "小七和弦" },
  { value: 8, en: "Power 5", zh: "强力五度" },
  { value: 9, en: "Octave", zh: "八度" },
];

export const ARPEGGIATORS: readonly LocalizedOption[] = [
  { value: 0, en: "Off", zh: "关闭" },
  { value: 1, en: "Up", zh: "上行" },
  { value: 2, en: "Down", zh: "下行" },
  { value: 3, en: "Up / down", zh: "上行 / 下行" },
  { value: 4, en: "Down / up", zh: "下行 / 上行" },
  { value: 5, en: "As played", zh: "演奏顺序" },
  { value: 6, en: "Seeded random", zh: "固定种子随机" },
];

export function optionsMarkup(options: readonly LocalizedOption[]): string {
  return options
    .map(
      ({ value, en, zh }) =>
        `<option value="${value}" data-en="${en}" data-zh="${zh}">${en}</option>`,
    )
    .join("");
}
