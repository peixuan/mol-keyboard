// SPDX-License-Identifier: Apache-2.0
import { KeyCode } from '@kit.InputKit';
import { AudioService, AudioSettings, RuntimeSnapshot } from '../audio/AudioService';

interface KeySpec {
  label: string;
  keyCode: number;
  note: number;
}

interface SelectOption {
  value: string;
}

const PRESETS: SelectOption[] = [
  { value: 'Grand Piano / 大钢琴' },
  { value: 'Electric Piano / 电钢琴' },
  { value: 'Harpsichord / 大键琴' },
  { value: 'Church Organ / 教堂风琴' },
  { value: 'Jazz Organ / 爵士风琴' },
  { value: 'Nylon Guitar / 尼龙吉他' },
  { value: 'Steel Guitar / 钢弦吉他' },
  { value: 'Violin / 小提琴' },
  { value: 'Cello / 大提琴' },
  { value: 'Flute / 长笛' },
  { value: 'Clarinet / 单簧管' },
  { value: 'Synth Lead / 合成器主音' },
  { value: 'Synth Pad / 合成器铺底' },
  { value: 'Synth Bass / 合成贝斯' },
  { value: 'Choir / 合唱' },
  { value: 'Vibraphone / 颤音琴' },
  { value: 'Harp / 竖琴' },
  { value: 'Music Box / 八音盒' },
];

const SCALES: SelectOption[] = [
  { value: 'Chromatic / 半音阶' },
  { value: 'Major / 大调' },
  { value: 'Natural minor / 自然小调' },
  { value: 'Major pentatonic / 大调五声' },
  { value: 'Minor pentatonic / 小调五声' },
  { value: 'Blues / 布鲁斯' },
  { value: 'Dorian / 多利亚' },
  { value: 'Mixolydian / 混合利底亚' },
];

const TONICS: SelectOption[] = [
  { value: 'C' }, { value: 'C♯' }, { value: 'D' }, { value: 'D♯' },
  { value: 'E' }, { value: 'F' }, { value: 'F♯' }, { value: 'G' },
  { value: 'G♯' }, { value: 'A' }, { value: 'A♯' }, { value: 'B' },
];

const SCALE_MAPPINGS: SelectOption[] = [
  { value: 'Nearest / 最近' },
  { value: 'Down / 向下' },
  { value: 'Up / 向上' },
];

const CHORDS: SelectOption[] = [
  { value: 'Off / 关闭' },
  { value: 'Major / 大三和弦' },
  { value: 'Minor / 小三和弦' },
  { value: 'Suspended 2 / 挂二' },
  { value: 'Suspended 4 / 挂四' },
  { value: 'Dominant 7 / 属七' },
  { value: 'Major 7 / 大七' },
  { value: 'Minor 7 / 小七' },
  { value: 'Power 5 / 强力五度' },
  { value: 'Octave / 八度' },
];

const ARPEGGIATORS: SelectOption[] = [
  { value: 'Off / 关闭' },
  { value: 'Up / 上行' },
  { value: 'Down / 下行' },
  { value: 'Up / down / 上下行' },
  { value: 'Down / up / 下上行' },
  { value: 'As played / 演奏顺序' },
  { value: 'Seeded random / 固定随机' },
];

const ARPEGGIATOR_RATES: SelectOption[] = [
  { value: '1/4' }, { value: '1/8' }, { value: '1/8T' },
  { value: '1/16' }, { value: '1/16T' }, { value: '1/32' },
];

const PORTAMENTO_MODES: SelectOption[] = [
  { value: 'Off / 关闭' },
  { value: 'Legato / 连奏' },
  { value: 'Always / 始终' },
];

const METER_NUMERATORS: SelectOption[] = [
  { value: '2' }, { value: '3' }, { value: '4' }, { value: '5' },
  { value: '6' }, { value: '7' }, { value: '9' }, { value: '12' },
];

const METER_DENOMINATORS: SelectOption[] = [{ value: '4' }, { value: '8' }];

const KEYBOARD_KEYS: KeySpec[] = [
  { label: '1', keyCode: KeyCode.KEYCODE_1, note: 48 },
  { label: '2', keyCode: KeyCode.KEYCODE_2, note: 49 },
  { label: '3', keyCode: KeyCode.KEYCODE_3, note: 50 },
  { label: '4', keyCode: KeyCode.KEYCODE_4, note: 51 },
  { label: '5', keyCode: KeyCode.KEYCODE_5, note: 52 },
  { label: '6', keyCode: KeyCode.KEYCODE_6, note: 53 },
  { label: '7', keyCode: KeyCode.KEYCODE_7, note: 54 },
  { label: '8', keyCode: KeyCode.KEYCODE_8, note: 55 },
  { label: '9', keyCode: KeyCode.KEYCODE_9, note: 56 },
  { label: '0', keyCode: KeyCode.KEYCODE_0, note: 57 },
  { label: 'Q', keyCode: KeyCode.KEYCODE_Q, note: 58 },
  { label: 'W', keyCode: KeyCode.KEYCODE_W, note: 59 },
  { label: 'E', keyCode: KeyCode.KEYCODE_E, note: 60 },
  { label: 'R', keyCode: KeyCode.KEYCODE_R, note: 61 },
  { label: 'T', keyCode: KeyCode.KEYCODE_T, note: 62 },
  { label: 'Y', keyCode: KeyCode.KEYCODE_Y, note: 63 },
  { label: 'U', keyCode: KeyCode.KEYCODE_U, note: 64 },
  { label: 'I', keyCode: KeyCode.KEYCODE_I, note: 65 },
  { label: 'O', keyCode: KeyCode.KEYCODE_O, note: 66 },
  { label: 'P', keyCode: KeyCode.KEYCODE_P, note: 67 },
  { label: 'A', keyCode: KeyCode.KEYCODE_A, note: 68 },
  { label: 'S', keyCode: KeyCode.KEYCODE_S, note: 69 },
  { label: 'D', keyCode: KeyCode.KEYCODE_D, note: 70 },
  { label: 'F', keyCode: KeyCode.KEYCODE_F, note: 71 },
  { label: 'G', keyCode: KeyCode.KEYCODE_G, note: 72 },
  { label: 'H', keyCode: KeyCode.KEYCODE_H, note: 73 },
  { label: 'J', keyCode: KeyCode.KEYCODE_J, note: 74 },
  { label: 'K', keyCode: KeyCode.KEYCODE_K, note: 75 },
  { label: 'L', keyCode: KeyCode.KEYCODE_L, note: 76 },
  { label: ';', keyCode: KeyCode.KEYCODE_SEMICOLON, note: 77 },
];

@Entry
@Component
struct Index {
  private readonly audio: AudioService = AudioService.getInstance();
  private readonly hardwareGestures: Map<number, number> = new Map<number, number>();
  private readonly touchGestures: Map<number, number> = new Map<number, number>();
  private gestureCounter: number = 1;
  private refreshTimer: number = -1;

  @State active: boolean = false;
  @State userStarted: boolean = false;
  @State fastPathActive: boolean = false;
  @State latencyFallbackUsed: boolean = false;
  @State statusMessage: string = 'Initializing native audio';
  @State sampleRate: number = 0;
  @State frameSize: number = 0;
  @State callbackCount: number = 0;
  @State underflows: number = 0;
  @State routeChanges: number = 0;
  @State interruptions: number = 0;
  @State renderFailures: number = 0;
  @State nonFiniteSamples: number = 0;
  @State backgroundTaskActive: boolean = false;
  @State transportRunning: boolean = false;
  @State recording: boolean = false;
  @State recordingAvailable: boolean = false;
  @State playbackRunning: boolean = false;

  @State preset: number = 0;
  @State masterGain: number = 0.82;
  @State octave: number = 0;
  @State transpose: number = 0;
  @State scaleIndex: number = 0;
  @State tonic: number = 0;
  @State scaleMapping: number = 0;
  @State chord: number = 0;
  @State arpeggiatorMode: number = 0;
  @State arpeggiatorRate: number = 4;
  @State arpeggiatorGate: number = 0.72;
  @State arpeggiatorOctaves: number = 1;
  @State tempo: number = 120;
  @State meterNumerator: number = 4;
  @State meterDenominator: number = 4;
  @State metronomeEnabled: boolean = false;
  @State metronomeLevel: number = 0.55;
  @State portamentoMode: number = 0;
  @State portamentoTimeMs: number = 80;

  aboutToAppear(): void {
    this.applySettings(this.audio.getSettings());
    this.refreshStatus();
    this.refreshTimer = setInterval(() => this.refreshStatus(), 125);
  }

  aboutToDisappear(): void {
    if (this.refreshTimer >= 0) {
      clearInterval(this.refreshTimer);
      this.refreshTimer = -1;
    }
    this.releaseAllGestures();
  }

  private applySettings(settings: AudioSettings): void {
    this.preset = settings.preset;
    this.masterGain = settings.masterGain;
    this.octave = settings.octave;
    this.transpose = settings.transpose;
    this.scaleIndex = settings.scale;
    this.tonic = settings.tonic;
    this.scaleMapping = settings.scaleMapping;
    this.chord = settings.chord;
    this.arpeggiatorMode = settings.arpeggiatorMode;
    this.arpeggiatorRate = settings.arpeggiatorRate;
    this.arpeggiatorGate = settings.arpeggiatorGate;
    this.arpeggiatorOctaves = settings.arpeggiatorOctaves;
    this.tempo = settings.tempo;
    this.meterNumerator = settings.meterNumerator;
    this.meterDenominator = settings.meterDenominator;
    this.metronomeEnabled = settings.metronomeEnabled;
    this.metronomeLevel = settings.metronomeLevel;
    this.portamentoMode = settings.portamentoMode;
    this.portamentoTimeMs = settings.portamentoTimeMs;
  }

  private refreshStatus(): void {
    const status: RuntimeSnapshot = this.audio.getSnapshot();
    this.active = status.active;
    this.userStarted = status.userStarted;
    this.fastPathActive = status.fastPathActive;
    this.latencyFallbackUsed = status.latencyFallbackUsed;
    this.statusMessage = status.statusMessage;
    this.sampleRate = status.sampleRate;
    this.frameSize = status.frameSize;
    this.callbackCount = status.callbackCount;
    this.underflows = status.underflowCount;
    this.routeChanges = status.routeChanges;
    this.interruptions = status.interruptions;
    this.renderFailures = status.renderFailures;
    this.nonFiniteSamples = status.nonFiniteSamples;
    this.backgroundTaskActive = status.backgroundTaskActive;
    this.transportRunning = status.transportRunning;
    this.recording = status.recording;
    this.recordingAvailable = status.recordingAvailable;
    this.playbackRunning = status.playbackRunning;
  }

  private nextGesture(): number {
    const gesture: number = this.gestureCounter;
    this.gestureCounter += 1;
    return gesture;
  }

  private pressTouchKey(index: number): void {
    if (this.touchGestures.has(index)) return;
    const gesture: number = this.nextGesture();
    if (this.audio.noteOn(KEYBOARD_KEYS[index].note, 0.82, gesture)) {
      this.touchGestures.set(index, gesture);
    }
  }

  private releaseTouchKey(index: number): void {
    const gesture: number | undefined = this.touchGestures.get(index);
    if (gesture === undefined) return;
    this.audio.noteOff(KEYBOARD_KEYS[index].note, gesture);
    this.touchGestures.delete(index);
  }

  private handleHardwareKey(event: KeyEvent): void {
    if (!this.active) return;
    if (event.keyCode === KeyCode.KEYCODE_SPACE) {
      this.audio.setSustain(event.type === KeyType.Down);
      event.stopPropagation();
      return;
    }
    let keyIndex: number = -1;
    for (let index: number = 0; index < KEYBOARD_KEYS.length; ++index) {
      if (KEYBOARD_KEYS[index].keyCode === event.keyCode) {
        keyIndex = index;
        break;
      }
    }
    if (keyIndex < 0) return;
    if (event.type === KeyType.Down && !this.hardwareGestures.has(event.keyCode)) {
      const gesture: number = this.nextGesture();
      if (this.audio.noteOn(KEYBOARD_KEYS[keyIndex].note, 0.82, gesture)) {
        this.hardwareGestures.set(event.keyCode, gesture);
      }
    } else if (event.type === KeyType.Up) {
      const gesture: number | undefined = this.hardwareGestures.get(event.keyCode);
      if (gesture !== undefined) {
        this.audio.noteOff(KEYBOARD_KEYS[keyIndex].note, gesture);
        this.hardwareGestures.delete(event.keyCode);
      }
    }
    event.stopPropagation();
  }

  private releaseAllGestures(): void {
    this.hardwareGestures.forEach((gesture: number, keyCode: number): void => {
      for (let index: number = 0; index < KEYBOARD_KEYS.length; ++index) {
        if (KEYBOARD_KEYS[index].keyCode === keyCode) {
          this.audio.noteOff(KEYBOARD_KEYS[index].note, gesture);
          break;
        }
      }
    });
    this.touchGestures.forEach((gesture: number, index: number): void => {
      this.audio.noteOff(KEYBOARD_KEYS[index].note, gesture);
    });
    this.hardwareGestures.clear();
    this.touchGestures.clear();
    this.audio.setSustain(false);
  }

  private applyScale(): void {
    this.audio.setScale(this.scaleIndex, this.tonic, this.scaleMapping);
  }

  private applyArpeggiator(): void {
    this.audio.setArpeggiator(this.arpeggiatorMode, this.arpeggiatorRate,
      this.arpeggiatorGate, this.arpeggiatorOctaves);
  }

  private meterNumeratorIndex(): number {
    const allowed: number[] = [2, 3, 4, 5, 6, 7, 9, 12];
    return Math.max(0, allowed.indexOf(this.meterNumerator));
  }

  build() {
    Scroll() {
      Column({ space: 14 }) {
        Column({ space: 4 }) {
          Text('MoL Keyboard / 张多少的键盘')
            .fontSize(27)
            .fontWeight(FontWeight.Bold)
            .fontColor('#F5F7FA')
          Text('Native OHAudio · 30-key instrument · Space = sustain')
            .fontSize(13)
            .fontColor('#AAB7C4')
          Text(this.statusMessage)
            .fontSize(14)
            .fontColor(this.active ? '#70E1B2' : '#FFCC7A')
        }
        .width('100%')
        .alignItems(HorizontalAlign.Start)

        Row({ space: 10 }) {
          Button(this.active ? 'Audio Running / 音频运行中' : 'Start Audio / 启动音频')
            .defaultFocus(true)
            .enabled(!this.active)
            .onClick((): void => {
              void this.audio.startByUser().then(() => this.refreshStatus());
            })
          Button('Stop / 停止')
            .enabled(this.userStarted)
            .onClick((): void => {
              this.releaseAllGestures();
              void this.audio.stopByUser().then(() => this.refreshStatus());
            })
        }
        .width('100%')

        Row({ space: 10 }) {
          Text(this.fastPathActive ? 'FAST path' : 'NORMAL path')
            .fontColor(this.fastPathActive ? '#70E1B2' : '#FFCC7A')
          Text(`${this.sampleRate} Hz · ${this.frameSize} frames`)
            .fontColor('#CBD5DF')
          if (this.latencyFallbackUsed) {
            Text('fast unavailable → fallback')
              .fontColor('#FFCC7A')
          }
        }
        .width('100%')

        Text('Instrument / 乐器')
          .fontSize(19)
          .fontWeight(FontWeight.Medium)
          .fontColor('#F5F7FA')
          .width('100%')
        Select(PRESETS)
          .selected(this.preset)
          .value(PRESETS[this.preset].value)
          .width('100%')
          .onSelect((index: number): void => {
            this.preset = index;
            this.audio.setPreset(index);
          })

        Text(`Master gain / 主音量 ${Math.round(this.masterGain * 100)}%`)
          .fontColor('#CBD5DF')
          .width('100%')
        Slider({ value: this.masterGain, min: 0, max: 1, step: 0.01 })
          .onChange((value: number): void => {
            this.masterGain = value;
            this.audio.setMasterGain(value);
          })

        Row({ space: 10 }) {
          Text(`Octave / 八度 ${this.octave}`)
            .fontColor('#CBD5DF')
          Button('−').enabled(this.octave > -2).onClick((): void => {
            this.octave -= 1;
            this.audio.setOctave(this.octave);
          })
          Button('+').enabled(this.octave < 2).onClick((): void => {
            this.octave += 1;
            this.audio.setOctave(this.octave);
          })
          Text(`Transpose / 移调 ${this.transpose}`)
            .fontColor('#CBD5DF')
          Button('−').enabled(this.transpose > -12).onClick((): void => {
            this.transpose -= 1;
            this.audio.setTranspose(this.transpose);
          })
          Button('+').enabled(this.transpose < 12).onClick((): void => {
            this.transpose += 1;
            this.audio.setTranspose(this.transpose);
          })
        }
        .width('100%')

        Text('Play / 演奏')
          .fontSize(19)
          .fontWeight(FontWeight.Medium)
          .fontColor('#F5F7FA')
          .width('100%')
        Grid() {
          ForEach(KEYBOARD_KEYS, (key: KeySpec, index: number) => {
            GridItem() {
              Button(`${key.label}\n${key.note}`)
                .width('100%')
                .height(54)
                .fontSize(13)
                .enabled(this.active)
                .onTouch((event: TouchEvent): void => {
                  if (event.type === TouchType.Down) {
                    this.pressTouchKey(index);
                  } else if (event.type === TouchType.Up || event.type === TouchType.Cancel) {
                    this.releaseTouchKey(index);
                  }
                })
            }
          }, (key: KeySpec): string => key.label)
        }
        .columnsTemplate('repeat(5, 1fr)')
        .columnsGap(7)
        .rowsGap(7)
        .width('100%')

        Text('Harmony / 音乐映射')
          .fontSize(19)
          .fontWeight(FontWeight.Medium)
          .fontColor('#F5F7FA')
          .width('100%')
        Row({ space: 8 }) {
          Select(SCALES)
            .selected(this.scaleIndex)
            .value(SCALES[this.scaleIndex].value)
            .layoutWeight(2)
            .onSelect((index: number): void => {
              this.scaleIndex = index;
              this.applyScale();
            })
          Select(TONICS)
            .selected(this.tonic)
            .value(TONICS[this.tonic].value)
            .layoutWeight(1)
            .onSelect((index: number): void => {
              this.tonic = index;
              this.applyScale();
            })
          Select(SCALE_MAPPINGS)
            .selected(this.scaleMapping)
            .value(SCALE_MAPPINGS[this.scaleMapping].value)
            .layoutWeight(2)
            .onSelect((index: number): void => {
              this.scaleMapping = index;
              this.applyScale();
            })
        }
        .width('100%')
        Select(CHORDS)
          .selected(this.chord)
          .value(CHORDS[this.chord].value)
          .width('100%')
          .onSelect((index: number): void => {
            this.chord = index;
            this.audio.setChord(index);
          })

        Text('Arpeggiator / 琶音器')
          .fontSize(19)
          .fontWeight(FontWeight.Medium)
          .fontColor('#F5F7FA')
          .width('100%')
        Row({ space: 8 }) {
          Select(ARPEGGIATORS)
            .selected(this.arpeggiatorMode)
            .value(ARPEGGIATORS[this.arpeggiatorMode].value)
            .layoutWeight(2)
            .onSelect((index: number): void => {
              this.arpeggiatorMode = index;
              this.applyArpeggiator();
            })
          Select(ARPEGGIATOR_RATES)
            .selected(this.arpeggiatorRate)
            .value(ARPEGGIATOR_RATES[this.arpeggiatorRate].value)
            .layoutWeight(1)
            .onSelect((index: number): void => {
              this.arpeggiatorRate = index;
              this.applyArpeggiator();
            })
          Button(`Oct ${this.arpeggiatorOctaves}`)
            .onClick((): void => {
              this.arpeggiatorOctaves = this.arpeggiatorOctaves >= 4 ? 1 :
                this.arpeggiatorOctaves + 1;
              this.applyArpeggiator();
            })
        }
        .width('100%')
        Text(`Gate / 门限 ${Math.round(this.arpeggiatorGate * 100)}%`)
          .fontColor('#CBD5DF')
          .width('100%')
        Slider({ value: this.arpeggiatorGate, min: 0.05, max: 1, step: 0.01 })
          .onChange((value: number): void => {
            this.arpeggiatorGate = value;
            this.applyArpeggiator();
          })

        Text('Transport / 走带与录音')
          .fontSize(19)
          .fontWeight(FontWeight.Medium)
          .fontColor('#F5F7FA')
          .width('100%')
        Text(`Tempo / 速度 ${Math.round(this.tempo)} BPM`)
          .fontColor('#CBD5DF')
          .width('100%')
        Slider({ value: this.tempo, min: 30, max: 300, step: 1 })
          .onChange((value: number): void => {
            this.tempo = value;
            this.audio.setTempo(value);
          })
        Row({ space: 8 }) {
          Text('Meter / 拍号').fontColor('#CBD5DF')
          Select(METER_NUMERATORS)
            .selected(this.meterNumeratorIndex())
            .value(`${this.meterNumerator}`)
            .onSelect((index: number): void => {
              const values: number[] = [2, 3, 4, 5, 6, 7, 9, 12];
              this.meterNumerator = values[index];
              this.audio.setTimeSignature(this.meterNumerator, this.meterDenominator);
            })
          Text('/').fontColor('#CBD5DF')
          Select(METER_DENOMINATORS)
            .selected(this.meterDenominator === 4 ? 0 : 1)
            .value(`${this.meterDenominator}`)
            .onSelect((index: number): void => {
              this.meterDenominator = index === 0 ? 4 : 8;
              this.audio.setTimeSignature(this.meterNumerator, this.meterDenominator);
            })
          Text('Metronome / 节拍器').fontColor('#CBD5DF')
          Toggle({ type: ToggleType.Switch, isOn: this.metronomeEnabled })
            .onChange((enabled: boolean): void => {
              this.metronomeEnabled = enabled;
              this.audio.setMetronome(enabled, this.metronomeLevel);
            })
        }
        .width('100%')
        Text(`Metronome level / 节拍音量 ${Math.round(this.metronomeLevel * 100)}%`)
          .fontColor('#CBD5DF')
          .width('100%')
        Slider({ value: this.metronomeLevel, min: 0, max: 1, step: 0.01 })
          .onChange((value: number): void => {
            this.metronomeLevel = value;
            this.audio.setMetronome(this.metronomeEnabled, value);
          })
        Row({ space: 8 }) {
          Button(this.transportRunning ? 'Stop Transport / 停止走带' : 'Start Transport / 开始走带')
            .enabled(this.active)
            .onClick((): void => {
              this.audio.toggleTransport();
              this.refreshStatus();
            })
          Button(this.recording ? 'Stop Record / 停止录音' : 'Record / 录音')
            .enabled(this.active)
            .onClick((): void => {
              this.audio.toggleRecording();
              this.refreshStatus();
            })
          Button(this.playbackRunning ? 'Stop Playback / 停止回放' : 'Playback / 回放')
            .enabled(this.active && (this.recordingAvailable || this.playbackRunning))
            .onClick((): void => {
              if (this.playbackRunning) {
                this.audio.stopPlayback();
              } else {
                void this.audio.startPlayback();
              }
              this.refreshStatus();
            })
        }
        .width('100%')

        Text('Portamento / 滑音')
          .fontSize(19)
          .fontWeight(FontWeight.Medium)
          .fontColor('#F5F7FA')
          .width('100%')
        Select(PORTAMENTO_MODES)
          .selected(this.portamentoMode)
          .value(PORTAMENTO_MODES[this.portamentoMode].value)
          .width('100%')
          .onSelect((index: number): void => {
            this.portamentoMode = index;
            this.audio.setPortamento(index, this.portamentoTimeMs);
          })
        Text(`Glide time / 滑音时间 ${Math.round(this.portamentoTimeMs)} ms`)
          .fontColor('#CBD5DF')
          .width('100%')
        Slider({ value: this.portamentoTimeMs, min: 0, max: 2000, step: 5 })
          .onChange((value: number): void => {
            this.portamentoTimeMs = value;
            this.audio.setPortamento(this.portamentoMode, value);
          })

        Text('Diagnostics / 诊断')
          .fontSize(19)
          .fontWeight(FontWeight.Medium)
          .fontColor('#F5F7FA')
          .width('100%')
        Text(`callbacks ${this.callbackCount} · underflows ${this.underflows} · ` +
          `routes ${this.routeChanges} · interruptions ${this.interruptions}`)
          .fontColor('#CBD5DF')
          .width('100%')
        Text(`render failures ${this.renderFailures} · non-finite samples ${this.nonFiniteSamples} · ` +
          `continuous task ${this.backgroundTaskActive ? 'active' : 'idle'}`)
          .fontColor('#CBD5DF')
          .width('100%')
        Text('Bluetooth audio follows the system route and may add substantial latency. ' +
          'Hardware keyboard performance is foreground-only.')
          .fontSize(12)
          .fontColor('#8FA0B2')
          .width('100%')
      }
      .width('100%')
      .padding(18)
      .alignItems(HorizontalAlign.Start)
      .onKeyEvent((event: KeyEvent): void => this.handleHardwareKey(event))
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#10151C')
    .scrollBar(BarState.Auto)
  }
}
