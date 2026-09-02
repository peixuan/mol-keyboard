import "./styles.css";
import { type EngineEvent, MolAudioEngine } from "./audio-engine";
import { ARPEGGIATORS, CHORDS, PRESETS, SCALES, optionsMarkup } from "./catalog";
import {
  BINDING_BY_CODE,
  BINDING_BY_NOTE,
  KEY_BINDINGS,
  KEY_RECTANGLES,
  KEYBOARD_HEIGHT,
  KEYBOARD_WIDTH,
  noteAtPoint,
  noteName,
} from "./keyboard";

interface ActiveGesture {
  readonly id: number;
  readonly note: number;
}

const template = document.createElement("template");
template.innerHTML = `
  <main class="app-shell">
    <header class="masthead">
      <a class="wordmark" href="#instrument" aria-label="MoL Keyboard home">
        <span class="wordmark-mark" aria-hidden="true">M</span>
        <span>MoL Keyboard</span>
      </a>
      <p class="edition">Web instrument · 浏览器乐器</p>
    </header>

    <section class="hero" aria-labelledby="hero-title">
      <div class="hero-copy">
        <p class="eyebrow">Thirty notes. One honest audio path.</p>
        <h1 id="hero-title">Play the<br /><em>browser.</em></h1>
        <p class="lede">
          A tactile polyphonic instrument rendered by the same C audio core as desktop and embedded builds.
          No samples, no cloud round-trip.
        </p>
      </div>
      <div class="start-panel">
        <p class="panel-label">Audio engine</p>
        <button class="start-button" type="button" data-action="start">
          <span>Start audio</span><span aria-hidden="true">→</span>
        </button>
        <div class="engine-state">
          <span class="state-dot" aria-hidden="true"></span>
          <span data-status role="status" aria-live="polite">Waiting for a gesture</span>
        </div>
        <dl class="spec-list">
          <div><dt>Core</dt><dd>WebAssembly</dd></div>
          <div><dt>Thread</dt><dd>AudioWorklet</dd></div>
          <div><dt>Voices</dt><dd>32 polyphonic</dd></div>
        </dl>
      </div>
    </section>

    <nav class="mode-bar" aria-label="Application mode and connection">
      <div class="mode-switch" aria-label="Interface mode">
        <button type="button" data-mode="explore" aria-pressed="true" data-en="Explore" data-zh="探索">Explore</button>
        <button type="button" data-mode="studio" aria-pressed="false" data-en="Studio" data-zh="工作室">Studio</button>
      </div>
      <label><span data-en="Backend" data-zh="后端">Backend</span>
        <select data-backend aria-label="Audio backend">
          <option value="standalone" data-en="This browser" data-zh="当前浏览器">This browser</option>
          <option value="service" data-en="Desktop service" data-zh="桌面服务">Desktop service</option>
          <option value="esp32" data-en="ESP32 device" data-zh="ESP32 设备">ESP32 device</option>
        </select>
      </label>
      <label><span data-en="Language" data-zh="语言">Language</span>
        <select data-language aria-label="Language">
          <option value="en">English</option>
          <option value="zh">中文</option>
        </select>
      </label>
    </nav>

    <section class="instrument" id="instrument" aria-labelledby="instrument-title">
      <div class="instrument-heading">
        <div>
          <p class="eyebrow">C4 — F6</p>
          <h2 id="instrument-title">Keyboard</h2>
        </div>
        <div class="keyboard-tools" aria-label="Keyboard navigation">
          <button type="button" class="tool-button" data-scroll="left" aria-label="Scroll keyboard left">←</button>
          <button type="button" class="tool-button" data-scroll="right" aria-label="Scroll keyboard right">→</button>
          <button type="button" class="panic-button" data-action="release">Release all</button>
        </div>
      </div>
      <p class="hint"><kbd>Z</kbd> through <kbd>[</kbd> follows two piano rows. Touch and mouse support independent notes.</p>
      <div class="keyboard-frame">
        <div class="keyboard-scroll" data-keyboard-scroll>
          <canvas
            data-keyboard
            role="application"
            tabindex="0"
            aria-label="Thirty-note polyphonic keyboard from C4 to F6. Use the displayed computer keys or pointer input."
          ></canvas>
        </div>
      </div>
      <div class="accessible-keys" data-accessible-keys aria-label="Accessible piano keys"></div>
    </section>

    <section class="control-room" aria-labelledby="controls-title">
      <div class="control-heading">
        <div>
          <p class="eyebrow" data-en="Shape the instrument" data-zh="塑造你的乐器">Shape the instrument</p>
          <h2 id="controls-title" data-en="Controls" data-zh="控制">Controls</h2>
        </div>
        <p class="voice-readout"><strong data-active-voices>0</strong> <span data-en="actual voices" data-zh="实际声部">actual voices</span></p>
      </div>

      <div class="explore-controls control-grid">
        <label class="control-card"><span data-en="Instrument" data-zh="音色">Instrument</span>
          <select data-control="preset">${optionsMarkup(PRESETS)}</select>
        </label>
        <label class="control-card"><span data-en="Scale" data-zh="音阶">Scale</span>
          <select data-control="scale">${optionsMarkup(SCALES)}</select>
        </label>
        <label class="control-card"><span data-en="Tonic" data-zh="主音">Tonic</span>
          <select data-control="tonic">
            <option value="0">C</option><option value="1">C♯</option><option value="2">D</option>
            <option value="3">D♯</option><option value="4">E</option><option value="5">F</option>
            <option value="6">F♯</option><option value="7">G</option><option value="8">G♯</option>
            <option value="9">A</option><option value="10">A♯</option><option value="11">B</option>
          </select>
        </label>
        <label class="control-card"><span data-en="Octave" data-zh="八度">Octave</span>
          <select data-control="octave">
            <option value="-2">−2</option><option value="-1">−1</option><option value="0" selected>0</option>
            <option value="1">+1</option><option value="2">+2</option>
          </select>
        </label>
        <label class="control-card range-card"><span><span data-en="Master volume" data-zh="主音量">Master volume</span><output data-output="volume">72%</output></span>
          <input data-control="volume" type="range" min="0" max="1" value="0.72" step="0.01" />
        </label>
        <label class="control-card toggle-card"><span data-en="Metronome" data-zh="节拍器">Metronome</span>
          <input data-control="metronome" type="checkbox" role="switch" />
        </label>
      </div>

      <div class="transport-controls" aria-label="Recording and playback">
        <button type="button" data-transport="record" class="record-button"><span class="record-dot" aria-hidden="true"></span><span data-en="Record" data-zh="录音">Record</span></button>
        <button type="button" data-transport="stop" data-en="Stop" data-zh="停止">Stop</button>
        <button type="button" data-transport="play" data-en="Play back" data-zh="回放">Play back</button>
        <span data-recording-state role="status" data-en="No take yet" data-zh="尚无录音">No take yet</span>
      </div>

      <div class="studio-controls" data-studio hidden>
        <div class="studio-section">
          <h3 data-en="Space & effects" data-zh="空间与效果">Space & effects</h3>
          <div class="control-grid three-up">
            <label class="control-card range-card"><span><span>Chorus</span><output data-output="chorus">18%</output></span><input data-control="chorus" type="range" min="0" max="1" value="0.18" step="0.01" /></label>
            <label class="control-card range-card"><span><span>Delay</span><output data-output="delay">12%</output></span><input data-control="delay" type="range" min="0" max="1" value="0.12" step="0.01" /></label>
            <label class="control-card range-card"><span><span>Reverb</span><output data-output="reverb">22%</output></span><input data-control="reverb" type="range" min="0" max="1" value="0.22" step="0.01" /></label>
          </div>
        </div>
        <div class="studio-section">
          <h3 data-en="Harmony & movement" data-zh="和声与运动">Harmony & movement</h3>
          <div class="control-grid">
            <label class="control-card"><span data-en="Chord" data-zh="和弦">Chord</span><select data-control="chord">${optionsMarkup(CHORDS)}</select></label>
            <label class="control-card"><span data-en="Arpeggiator" data-zh="琶音器">Arpeggiator</span><select data-control="arpeggiator">${optionsMarkup(ARPEGGIATORS)}</select></label>
            <label class="control-card"><span data-en="Rate" data-zh="速率">Rate</span><select data-control="arp-rate"><option value="0">1/4</option><option value="1">1/8</option><option value="2">1/8T</option><option value="3" selected>1/16</option><option value="4">1/16T</option><option value="5">1/32</option></select></label>
            <label class="control-card"><span data-en="Arp octaves" data-zh="琶音八度">Arp octaves</span><select data-control="arp-octaves"><option value="1">1</option><option value="2" selected>2</option><option value="3">3</option><option value="4">4</option></select></label>
            <label class="control-card range-card"><span><span>Gate</span><output data-output="gate">70%</output></span><input data-control="gate" type="range" min="0.05" max="1" value="0.7" step="0.05" /></label>
            <label class="control-card"><span>BPM</span><input data-control="tempo" type="number" min="30" max="300" value="120" step="1" /></label>
            <label class="control-card"><span data-en="Time signature" data-zh="拍号">Time signature</span><select data-control="time-signature"><option value="4/4">4/4</option><option value="3/4">3/4</option><option value="6/8">6/8</option><option value="5/4">5/4</option></select></label>
            <label class="control-card"><span>Portamento</span><select data-control="portamento"><option value="0" data-en="Off" data-zh="关闭">Off</option><option value="1" data-en="Legato" data-zh="连奏">Legato</option><option value="2" data-en="Always" data-zh="始终">Always</option></select></label>
            <label class="control-card range-card"><span><span data-en="Glide time" data-zh="滑音时间">Glide time</span><output data-output="portamento-time">120 ms</output></span><input data-control="portamento-time" type="range" min="0" max="2000" value="120" step="10" /></label>
          </div>
        </div>
        <div class="studio-section diagnostics">
          <h3 data-en="Runtime facts" data-zh="运行时事实">Runtime facts</h3>
          <dl>
            <div><dt data-en="Input" data-zh="输入">Input</dt><dd>Keyboard + Pointer Events</dd></div>
            <div><dt data-en="Output" data-zh="输出">Output</dt><dd>Web Audio system default</dd></div>
            <div><dt data-en="Fast path" data-zh="快速路径">Fast path</dt><dd data-fast-path>MessagePort baseline</dd></div>
            <div><dt data-en="Persistence" data-zh="持久化">Persistence</dt><dd data-storage-state>Checking…</dd></div>
            <div><dt data-en="Core events" data-zh="核心事件">Core events</dt><dd data-event-count>0</dd></div>
          </dl>
        </div>
      </div>
    </section>

    <footer>
      <p>MoL / Matter of Latency</p>
      <p>Local-first · Offline-ready foundation</p>
    </footer>
  </main>
`;

class MolKeyboardApp extends HTMLElement {
  private readonly engine = new MolAudioEngine();
  private readonly activeKeys = new Map<string, ActiveGesture>();
  private readonly activePointers = new Map<number, ActiveGesture>();
  private readonly activeAccessible = new Map<number, ActiveGesture>();
  private readonly soundingGestures = new Set<number>();
  private readonly actualVoices = new Map<string, number>();
  private canvas: HTMLCanvasElement | undefined;
  private context: CanvasRenderingContext2D | undefined;
  private statusElement: HTMLElement | undefined;
  private startButton: HTMLButtonElement | undefined;
  private nextGestureId = 1;
  private coreEventCount = 0;
  private language: "en" | "zh" = navigator.language.toLowerCase().startsWith("zh") ? "zh" : "en";
  private connected = false;

  connectedCallback(): void {
    if (this.connected) return;
    this.connected = true;
    this.append(template.content.cloneNode(true));
    this.canvas = this.querySelector<HTMLCanvasElement>("[data-keyboard]") ?? undefined;
    this.context = this.canvas?.getContext("2d") ?? undefined;
    this.statusElement = this.querySelector<HTMLElement>("[data-status]") ?? undefined;
    this.startButton = this.querySelector<HTMLButtonElement>("[data-action='start']") ?? undefined;
    this.configureCanvas();
    this.createAccessibleKeys();
    this.bindEvents();
    this.bindControls();
    this.applyLanguage(this.language);
    this.updateRuntimeFacts();
    this.drawKeyboard();
  }

  disconnectedCallback(): void {
    this.releaseAll();
    void this.engine.close();
  }

  private bindEvents(): void {
    this.startButton?.addEventListener("click", () => void this.startAudio());
    this.querySelector("[data-action='release']")?.addEventListener("click", () => this.releaseAll());
    this.querySelector("[data-scroll='left']")?.addEventListener("click", () => this.scrollKeyboard(-1));
    this.querySelector("[data-scroll='right']")?.addEventListener("click", () => this.scrollKeyboard(1));

    this.canvas?.addEventListener("pointerdown", (event) => this.onPointerDown(event));
    this.canvas?.addEventListener("pointermove", (event) => this.onPointerMove(event));
    this.canvas?.addEventListener("pointerup", (event) => this.onPointerEnd(event));
    this.canvas?.addEventListener("pointercancel", (event) => this.onPointerEnd(event));
    this.canvas?.addEventListener("lostpointercapture", (event) => this.onPointerEnd(event));
    window.addEventListener("keydown", (event) => this.onKeyDown(event));
    window.addEventListener("keyup", (event) => this.onKeyUp(event));
    window.addEventListener("blur", () => this.releaseAll());
    window.addEventListener("pagehide", () => this.releaseAll());
    document.addEventListener("visibilitychange", () => {
      if (document.visibilityState === "hidden") this.releaseAll();
    });
    this.engine.addEventListener("engineevents", (event) => {
      const engineEvent = event as CustomEvent<readonly EngineEvent[]>;
      this.onEngineEvents(engineEvent.detail);
    });
    this.engine.addEventListener("statechange", () => this.onAudioReady());
  }

  private onAudioReady(): void {
    const sampleRate = this.engine.sampleRate;
    const rateLabel = sampleRate === undefined ? "worklet" : `${Math.round(sampleRate / 100) / 10} kHz worklet`;
    this.setStatus(`Audio ready · ${rateLabel}`, "ready");
    if (this.startButton !== undefined) {
      this.startButton.disabled = true;
      const label = this.startButton.querySelector("span");
      if (label !== null) label.textContent = this.language === "zh" ? "音频已就绪" : "Audio ready";
    }
  }

  private bindControls(): void {
    for (const button of this.querySelectorAll<HTMLButtonElement>("[data-mode]")) {
      button.addEventListener("click", () => this.setMode(button.dataset.mode === "studio"));
    }
    this.querySelector<HTMLSelectElement>("[data-language]")?.addEventListener("change", (event) => {
      const value = (event.currentTarget as HTMLSelectElement).value;
      this.applyLanguage(value === "zh" ? "zh" : "en");
    });
    this.querySelector<HTMLSelectElement>("[data-backend]")?.addEventListener("change", (event) => {
      const value = (event.currentTarget as HTMLSelectElement).value;
      if (value !== "standalone") {
        this.setStatus(
          value === "service"
            ? "Desktop service controller selected · connection required"
            : "ESP32 controller selected · connection required",
          "loading",
        );
      } else {
        this.setStatus("Standalone Web audio selected", "ready");
      }
    });

    this.bindSelect("preset", (value) => this.engine.setPreset(value));
    this.bindSelect("octave", (value) => this.engine.setOctave(value));
    this.bindSelect("chord", (value) => this.engine.setChord(value));
    this.bindSelect("tempo", (value) => this.engine.setTempo(value));
    for (const control of ["scale", "tonic"] as const) {
      this.control(control)?.addEventListener("change", () => {
        this.runControl(
          "scale",
          this.engine.setScale(this.controlNumber("scale"), this.controlNumber("tonic"), 0),
        );
      });
    }
    for (const control of ["arpeggiator", "arp-rate", "arp-octaves", "gate"] as const) {
      this.control(control)?.addEventListener("change", () => this.submitArpeggiator());
    }
    this.control("time-signature")?.addEventListener("change", () => {
      const [numerator = 4, denominator = 4] = this.control("time-signature")!
        .value.split("/")
        .map(Number);
      this.runControl("time signature", this.engine.setTimeSignature(numerator, denominator));
    });
    for (const control of ["portamento", "portamento-time"] as const) {
      this.control(control)?.addEventListener("change", () => {
        this.runControl(
          "portamento",
          this.engine.setPortamento(
            this.controlNumber("portamento"),
            this.controlNumber("portamento-time"),
          ),
        );
      });
    }
    const metronome = this.querySelector<HTMLInputElement>("[data-control='metronome']");
    metronome?.addEventListener("change", () => {
      this.runControl("metronome", this.engine.setMetronome(metronome.checked, 0.5));
    });

    this.bindRange("volume", (value) => this.engine.setMasterGain(value));
    this.bindRange("chorus", (value) => this.engine.setParameter(3, value));
    this.bindRange("delay", (value) => this.engine.setParameter(6, value));
    this.bindRange("reverb", (value) => this.engine.setParameter(11, value));
    this.bindRange("gate", () => this.submitArpeggiator(), false);
    this.bindRange(
      "portamento-time",
      (value) => this.engine.setPortamento(this.controlNumber("portamento"), value),
      false,
    );

    this.querySelector("[data-transport='record']")?.addEventListener("click", () => {
      this.runControl("record", this.engine.action("record-start"));
    });
    this.querySelector("[data-transport='stop']")?.addEventListener("click", () => {
      this.runControl("stop recording", this.engine.action("record-stop"));
      this.runControl("stop playback", this.engine.action("playback-stop"));
    });
    this.querySelector("[data-transport='play']")?.addEventListener("click", () => {
      this.runControl("playback", this.engine.action("playback-start"));
    });
  }

  private bindSelect(control: string, submit: (value: number) => Promise<boolean>): void {
    this.control(control)?.addEventListener("change", () => {
      this.runControl(control, submit(this.controlNumber(control)));
    });
  }

  private bindRange(
    control: string,
    submit: (value: number) => Promise<boolean>,
    submitOnInput = true,
  ): void {
    const input = this.querySelector<HTMLInputElement>(`[data-control='${control}']`);
    if (input === null) return;
    const update = (): void => {
      const value = Number(input.value);
      const output = this.querySelector<HTMLOutputElement>(`[data-output='${control}']`);
      if (output !== null) {
        output.value = control === "portamento-time" ? `${value} ms` : `${Math.round(value * 100)}%`;
      }
      if (submitOnInput) this.runControl(control, submit(value));
    };
    input.addEventListener("input", update);
  }

  private submitArpeggiator(): Promise<boolean> {
    const operation = this.engine.setArpeggiator(
      this.controlNumber("arpeggiator"),
      this.controlNumber("arp-rate"),
      this.controlNumber("gate"),
      this.controlNumber("arp-octaves"),
      0x4d4f4c,
    );
    this.runControl("arpeggiator", operation);
    return operation;
  }

  private runControl(label: string, operation: Promise<boolean>): void {
    void operation
      .then((accepted) => {
        if (!accepted) this.setStatus(`${label} was rejected by the audio engine`, "error");
      })
      .catch((error: unknown) => {
        const message = error instanceof Error ? error.message : `${label} failed`;
        this.setStatus(message, "error");
      });
  }

  private control(name: string): HTMLInputElement | HTMLSelectElement | null {
    return this.querySelector<HTMLInputElement | HTMLSelectElement>(`[data-control='${name}']`);
  }

  private controlNumber(name: string): number {
    return Number(this.control(name)?.value ?? 0);
  }

  private setMode(studio: boolean): void {
    for (const button of this.querySelectorAll<HTMLButtonElement>("[data-mode]")) {
      button.setAttribute("aria-pressed", String((button.dataset.mode === "studio") === studio));
    }
    const panel = this.querySelector<HTMLElement>("[data-studio]");
    if (panel !== null) panel.hidden = !studio;
  }

  private applyLanguage(language: "en" | "zh"): void {
    this.language = language;
    document.documentElement.lang = language === "zh" ? "zh-CN" : "en";
    const select = this.querySelector<HTMLSelectElement>("[data-language]");
    if (select !== null) select.value = language;
    for (const element of this.querySelectorAll<HTMLElement>("[data-en][data-zh]")) {
      element.textContent = element.dataset[language] ?? element.textContent;
    }
  }

  private updateRuntimeFacts(): void {
    const isolated = window.crossOriginIsolated && typeof SharedArrayBuffer !== "undefined";
    const fastPath = this.querySelector<HTMLElement>("[data-fast-path]");
    if (fastPath !== null) {
      fastPath.textContent = isolated ? "SharedArrayBuffer available" : "MessagePort baseline";
    }
    const storage = this.querySelector<HTMLElement>("[data-storage-state]");
    if (storage !== null) {
      storage.textContent = "indexedDB" in window ? "IndexedDB available" : "Unavailable";
    }
  }

  private onEngineEvents(events: readonly EngineEvent[]): void {
    this.coreEventCount += events.length;
    for (const event of events) {
      const key = `${event.gestureId}:${event.note}`;
      if (event.type === 1) {
        this.actualVoices.set(key, event.note);
      } else if (event.type === 3 || event.type === 8) {
        this.actualVoices.delete(key);
      } else if (event.type === 7) {
        this.setRecordingState(event.detail !== 0 ? "recording" : "recorded");
      } else if (event.type === 14) {
        this.setRecordingState(event.detail !== 0 ? "playing" : "recorded");
      }
    }
    const voiceReadout = this.querySelector<HTMLElement>("[data-active-voices]");
    if (voiceReadout !== null) voiceReadout.textContent = String(this.actualVoices.size);
    const eventReadout = this.querySelector<HTMLElement>("[data-event-count]");
    if (eventReadout !== null) eventReadout.textContent = String(this.coreEventCount);
    if (this.actualVoices.size > 0) this.setStatus("Audio ready · playing", "ready");
    this.drawKeyboard();
  }

  private setRecordingState(state: "recording" | "recorded" | "playing"): void {
    const status = this.querySelector<HTMLElement>("[data-recording-state]");
    if (status === null) return;
    const labels = {
      recording: this.language === "zh" ? "正在录音" : "Recording…",
      recorded: this.language === "zh" ? "录音已保存在引擎中" : "Take held in the engine",
      playing: this.language === "zh" ? "正在回放" : "Playing take…",
    };
    status.textContent = labels[state];
    status.dataset.state = state;
  }

  private configureCanvas(): void {
    if (this.canvas === undefined) return;
    const scale = Math.max(1, Math.min(window.devicePixelRatio, 3));
    this.canvas.width = Math.round(KEYBOARD_WIDTH * scale);
    this.canvas.height = Math.round(KEYBOARD_HEIGHT * scale);
    this.canvas.style.width = `${KEYBOARD_WIDTH}px`;
    this.canvas.style.height = `${KEYBOARD_HEIGHT}px`;
    this.context?.setTransform(scale, 0, 0, scale, 0, 0);
  }

  private createAccessibleKeys(): void {
    const container = this.querySelector<HTMLElement>("[data-accessible-keys]");
    if (container === null) return;
    for (const binding of KEY_BINDINGS) {
      const button = document.createElement("button");
      button.type = "button";
      button.textContent = `${noteName(binding.note)} (${binding.label})`;
      button.setAttribute("aria-label", `Play ${noteName(binding.note)}, computer key ${binding.label}`);
      button.addEventListener("pointerdown", (event) => {
        event.preventDefault();
        const gesture = this.activate(binding.note);
        this.activeAccessible.set(binding.note, gesture);
      });
      const release = (): void => {
        const gesture = this.activeAccessible.get(binding.note);
        if (gesture !== undefined) this.deactivate(gesture);
        this.activeAccessible.delete(binding.note);
      };
      button.addEventListener("pointerup", release);
      button.addEventListener("pointercancel", release);
      button.addEventListener("blur", release);
      container.append(button);
    }
  }

  private async startAudio(): Promise<void> {
    if (this.startButton !== undefined) this.startButton.disabled = true;
    this.setStatus("Loading the WebAssembly audio core…", "loading");
    try {
      await this.engine.start();
      this.onAudioReady();
    } catch (error: unknown) {
      const message = error instanceof Error ? error.message : "Audio initialization failed.";
      this.setStatus(message, "error");
      if (this.startButton !== undefined) this.startButton.disabled = false;
      this.releaseAll();
    }
  }

  private activate(note: number): ActiveGesture {
    const gesture = { id: this.allocateGestureId(), note };
    this.drawKeyboard();
    void this.engine
      .start()
      .then(() => {
        if (!this.isGestureActive(gesture.id)) return;
        this.engine.noteOn(note, 0.82, gesture.id);
        this.soundingGestures.add(gesture.id);
      })
      .catch((error: unknown) => {
        const message = error instanceof Error ? error.message : "Audio initialization failed.";
        this.setStatus(message, "error");
        this.releaseAll();
      });
    return gesture;
  }

  private deactivate(gesture: ActiveGesture): void {
    if (this.soundingGestures.delete(gesture.id)) this.engine.noteOff(gesture.id);
    this.drawKeyboard();
  }

  private onKeyDown(event: KeyboardEvent): void {
    if (event.repeat || this.activeKeys.has(event.code) || this.isTypingTarget(event.target)) return;
    const binding = BINDING_BY_CODE.get(event.code);
    if (binding === undefined) return;
    event.preventDefault();
    const gesture = this.activate(binding.note);
    this.activeKeys.set(event.code, gesture);
    this.drawKeyboard();
  }

  private onKeyUp(event: KeyboardEvent): void {
    const gesture = this.activeKeys.get(event.code);
    if (gesture === undefined) return;
    event.preventDefault();
    this.activeKeys.delete(event.code);
    this.deactivate(gesture);
  }

  private onPointerDown(event: PointerEvent): void {
    if (this.canvas === undefined) return;
    event.preventDefault();
    this.canvas.setPointerCapture(event.pointerId);
    const note = this.noteFromPointer(event);
    if (note === undefined) return;
    const gesture = this.activate(note);
    this.activePointers.set(event.pointerId, gesture);
    this.drawKeyboard();
  }

  private onPointerMove(event: PointerEvent): void {
    if (!this.activePointers.has(event.pointerId)) return;
    event.preventDefault();
    const previous = this.activePointers.get(event.pointerId);
    const note = this.noteFromPointer(event);
    if (previous === undefined || note === previous.note) return;
    this.activePointers.delete(event.pointerId);
    this.deactivate(previous);
    if (note !== undefined) this.activePointers.set(event.pointerId, this.activate(note));
    this.drawKeyboard();
  }

  private onPointerEnd(event: PointerEvent): void {
    const gesture = this.activePointers.get(event.pointerId);
    if (gesture === undefined) return;
    this.activePointers.delete(event.pointerId);
    this.deactivate(gesture);
    if (this.canvas?.hasPointerCapture(event.pointerId) === true) {
      this.canvas.releasePointerCapture(event.pointerId);
    }
  }

  private noteFromPointer(event: PointerEvent): number | undefined {
    if (this.canvas === undefined) return undefined;
    const bounds = this.canvas.getBoundingClientRect();
    const x = ((event.clientX - bounds.left) / bounds.width) * KEYBOARD_WIDTH;
    const y = ((event.clientY - bounds.top) / bounds.height) * KEYBOARD_HEIGHT;
    return noteAtPoint(x, y);
  }

  private releaseAll(): void {
    this.activeKeys.clear();
    this.activePointers.clear();
    this.activeAccessible.clear();
    this.soundingGestures.clear();
    this.engine.allNotesOff();
    this.drawKeyboard();
    if (this.engine.state !== "idle" && this.engine.state !== "closed") {
      this.setStatus("Audio ready · all notes released", "ready");
    }
  }

  private isGestureActive(id: number): boolean {
    const contains = (gesture: ActiveGesture): boolean => gesture.id === id;
    return (
      [...this.activeKeys.values()].some(contains) ||
      [...this.activePointers.values()].some(contains) ||
      [...this.activeAccessible.values()].some(contains)
    );
  }

  private activeNotes(): ReadonlySet<number> {
    return new Set(this.actualVoices.values());
  }

  private allocateGestureId(): number {
    const id = this.nextGestureId;
    this.nextGestureId = id === 0xffffffff ? 1 : id + 1;
    return id;
  }

  private drawKeyboard(): void {
    const context = this.context;
    if (context === undefined) return;
    const active = this.activeNotes();
    context.clearRect(0, 0, KEYBOARD_WIDTH, KEYBOARD_HEIGHT);
    for (const key of KEY_RECTANGLES) {
      const selected = active.has(key.note);
      context.fillStyle = selected ? "#ff7458" : key.black ? "#17213b" : "#fffaf0";
      context.fillRect(key.x, key.y, key.width, key.height);
      context.strokeStyle = key.black ? "#fffaf0" : "#17213b";
      context.lineWidth = key.black ? 1.5 : 2;
      context.strokeRect(key.x + 1, key.y + 1, key.width - 2, key.height - 2);

      const binding = BINDING_BY_NOTE.get(key.note);
      if (binding === undefined) continue;
      const labelY = key.black ? key.height - 17 : key.height - 22;
      context.fillStyle = selected || key.black ? "#fffaf0" : "#17213b";
      context.font = "700 13px ui-monospace, SFMono-Regular, Consolas, monospace";
      context.textAlign = "center";
      context.fillText(binding.label, key.x + key.width / 2, labelY);
      if (!key.black) {
        context.fillStyle = selected ? "#fffaf0" : "#6e6a62";
        context.font = "600 10px ui-monospace, SFMono-Regular, Consolas, monospace";
        context.fillText(noteName(key.note), key.x + key.width / 2, labelY - 20);
      }
    }
  }

  private scrollKeyboard(direction: -1 | 1): void {
    const scroll = this.querySelector<HTMLElement>("[data-keyboard-scroll]");
    scroll?.scrollBy({ left: direction * Math.max(280, scroll.clientWidth * 0.7), behavior: "smooth" });
  }

  private setStatus(message: string, state: "loading" | "ready" | "error"): void {
    if (this.statusElement !== undefined) this.statusElement.textContent = message;
    this.querySelector(".engine-state")?.setAttribute("data-state", state);
  }

  private isTypingTarget(target: EventTarget | null): boolean {
    if (!(target instanceof HTMLElement)) return false;
    return target.isContentEditable || ["INPUT", "SELECT", "TEXTAREA"].includes(target.tagName);
  }
}

customElements.define("mol-keyboard-app", MolKeyboardApp);
