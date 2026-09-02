import "./styles.css";
import type { AudioBackend } from "./audio-backend";
import { type EngineEvent, MolAudioEngine } from "./audio-engine";
import { ARPEGGIATORS, CHORDS, PRESETS, SCALES, optionsMarkup } from "./catalog";
import {
  type WebSettings,
  listRecordings,
  loadRecording as loadStoredRecording,
  loadSettings,
  saveRecording,
  saveSettings,
} from "./persistence";
import { registerPwa } from "./pwa";
import { ServiceAudioEngine } from "./service-engine";
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

    <section class="connection-panel" data-service-connection hidden aria-labelledby="service-title">
      <div>
        <p class="eyebrow">Local authenticated control</p>
        <h2 id="service-title" data-en="Desktop service" data-zh="桌面服务">Desktop service</h2>
        <p data-en="Start mol-keyboardd with WebSocket control, then enter its printed endpoint and one-time session token. The token is never stored."
           data-zh="以 WebSocket 控制方式启动 mol-keyboardd，然后输入它输出的端点与一次性会话令牌。令牌绝不会被保存。">
          Start mol-keyboardd with WebSocket control, then enter its printed endpoint and one-time session token. The token is never stored.
        </p>
      </div>
      <div class="connection-fields">
        <label><span data-en="Endpoint" data-zh="端点">Endpoint</span>
          <input data-service-endpoint type="url" value="ws://127.0.0.1:8766/control" spellcheck="false" autocomplete="off" />
        </label>
        <label><span data-en="Session token" data-zh="会话令牌">Session token</span>
          <input data-service-token type="password" maxlength="128" autocomplete="off" />
        </label>
        <div class="connection-actions">
          <button type="button" data-service-connect data-en="Connect" data-zh="连接">Connect</button>
          <button type="button" data-service-disconnect data-en="Disconnect" data-zh="断开">Disconnect</button>
        </div>
        <output data-service-status role="status" aria-live="polite">Not connected</output>
      </div>
    </section>

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
      <div class="recording-library">
        <label><span data-en="Saved takes" data-zh="已保存录音">Saved takes</span>
          <select data-recording-list aria-label="Saved recordings"><option value="">—</option></select>
        </label>
        <button type="button" data-recording-load data-en="Load selected" data-zh="载入所选">Load selected</button>
        <output data-recording-storage>IndexedDB / OPFS</output>
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
            <div><dt data-en="Dropped commands" data-zh="丢弃命令">Dropped commands</dt><dd data-dropped-count>0</dd></div>
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
  private readonly standaloneEngine = new MolAudioEngine();
  private readonly serviceEngine = new ServiceAudioEngine();
  private engine: AudioBackend = this.standaloneEngine;
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
  private recording = false;
  private language: "en" | "zh" = navigator.language.toLowerCase().startsWith("zh") ? "zh" : "en";
  private connected = false;
  private settingsReady: Promise<void> = Promise.resolve();
  private configurationPromise: Promise<void> | undefined;
  private saveSettingsTimer: number | undefined;

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
    this.settingsReady = this.restoreSettings();
    void this.refreshRecordings();
    this.drawKeyboard();
  }

  disconnectedCallback(): void {
    this.releaseAll();
    void this.standaloneEngine.close();
    void this.serviceEngine.close();
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
    for (const backend of [this.standaloneEngine, this.serviceEngine]) {
      backend.addEventListener("engineevents", (event) => {
        if (backend !== this.engine) return;
        const engineEvent = event as CustomEvent<readonly EngineEvent[]>;
        this.onEngineEvents(engineEvent.detail);
      });
      backend.addEventListener("statechange", () => this.onEngineStateChanged(backend));
    }
  }

  private onAudioReady(): void {
    const sampleRate = this.engine.sampleRate;
    const backendLabel = this.engine === this.serviceEngine ? "desktop service" : "worklet";
    const rateLabel =
      sampleRate === undefined
        ? backendLabel
        : `${Math.round(sampleRate / 100) / 10} kHz ${backendLabel}`;
    this.setStatus(`Audio ready · ${rateLabel}`, "ready");
    if (this.startButton !== undefined) {
      this.startButton.disabled = true;
      const label = this.startButton.querySelector("span");
      if (label !== null) label.textContent = this.language === "zh" ? "音频已就绪" : "Audio ready";
    }
    this.updateRuntimeFacts();
  }

  private onEngineStateChanged(backend: AudioBackend): void {
    if (backend !== this.engine) return;
    if (backend.state === "running") {
      this.onAudioReady();
      return;
    }
    if (backend === this.serviceEngine) {
      this.setStatus("Desktop service disconnected", "error");
      if (this.startButton !== undefined) this.startButton.disabled = false;
      const connectionStatus = this.querySelector<HTMLOutputElement>("[data-service-status]");
      if (connectionStatus !== null) connectionStatus.value = "Not connected";
    }
  }

  private bindControls(): void {
    for (const button of this.querySelectorAll<HTMLButtonElement>("[data-mode]")) {
      button.addEventListener("click", () => {
        this.setMode(button.dataset.mode === "studio");
        this.scheduleSettingsSave();
      });
    }
    this.querySelector<HTMLSelectElement>("[data-language]")?.addEventListener("change", (event) => {
      const value = (event.currentTarget as HTMLSelectElement).value;
      this.applyLanguage(value === "zh" ? "zh" : "en");
      this.scheduleSettingsSave();
    });
    this.querySelector<HTMLSelectElement>("[data-backend]")?.addEventListener("change", (event) => {
      const value = (event.currentTarget as HTMLSelectElement).value;
      void this.selectBackend(value);
      this.scheduleSettingsSave();
    });
    this.querySelector("[data-service-connect]")?.addEventListener("click", () => {
      void this.connectService();
    });
    this.querySelector("[data-service-disconnect]")?.addEventListener("click", () => {
      void this.disconnectService();
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
      this.runControl("record", this.startRecording());
    });
    this.querySelector("[data-transport='stop']")?.addEventListener("click", () => {
      void this.stopTransport();
    });
    this.querySelector("[data-transport='play']")?.addEventListener("click", () => {
      this.runControl("playback", this.playRecording());
    });
    this.querySelector("[data-recording-load]")?.addEventListener("click", () => {
      void this.loadSelectedRecording();
    });
    this.addEventListener("change", () => this.scheduleSettingsSave());
  }

  private bindSelect(control: string, submit: (value: number) => Promise<boolean>): void {
    this.control(control)?.addEventListener("change", () => {
      this.runControl(control, submit(this.controlNumber(control)));
    });
  }

  private async selectBackend(value: string): Promise<void> {
    this.releaseAll();
    this.configurationPromise = undefined;
    this.actualVoices.clear();
    const panel = this.querySelector<HTMLElement>("[data-service-connection]");
    if (value === "service") {
      this.engine = this.serviceEngine;
      if (panel !== null) panel.hidden = false;
      this.setStatus(
        this.serviceEngine.connected
          ? "Desktop service connected"
          : "Desktop service controller selected · connection required",
        this.serviceEngine.connected ? "ready" : "loading",
      );
      if (this.startButton !== undefined) this.startButton.disabled = !this.serviceEngine.connected;
    } else if (value === "standalone") {
      this.engine = this.standaloneEngine;
      if (panel !== null) panel.hidden = true;
      await this.serviceEngine.close();
      this.setStatus("Standalone Web audio selected", "ready");
      if (this.startButton !== undefined) this.startButton.disabled = false;
    } else {
      this.engine = this.serviceEngine;
      if (panel !== null) panel.hidden = true;
      await this.serviceEngine.close();
      this.setStatus("ESP32 controller transport is not connected", "loading");
      if (this.startButton !== undefined) this.startButton.disabled = true;
    }
    await this.refreshRecordings();
    this.updateRuntimeFacts();
    this.drawKeyboard();
  }

  private async connectService(): Promise<void> {
    const endpoint = this.querySelector<HTMLInputElement>("[data-service-endpoint]");
    const token = this.querySelector<HTMLInputElement>("[data-service-token]");
    const status = this.querySelector<HTMLOutputElement>("[data-service-status]");
    const connect = this.querySelector<HTMLButtonElement>("[data-service-connect]");
    if (endpoint === null || token === null) return;
    if (connect !== null) connect.disabled = true;
    if (status !== null) status.value = "Connecting…";
    this.setStatus("Connecting to the desktop service…", "loading");
    try {
      await this.serviceEngine.connect(endpoint.value.trim(), token.value.trim());
      token.value = "";
      this.engine = this.serviceEngine;
      this.configurationPromise = this.applyCurrentSettings();
      await this.configurationPromise;
      if (status !== null) status.value = "Connected · authenticated loopback";
      await this.refreshRecordings();
      this.onAudioReady();
    } catch (error: unknown) {
      const message = error instanceof Error ? error.message : "Could not connect to the service";
      if (status !== null) status.value = message;
      this.setStatus(message, "error");
      if (this.startButton !== undefined) this.startButton.disabled = true;
    } finally {
      if (connect !== null) connect.disabled = false;
    }
  }

  private async disconnectService(): Promise<void> {
    this.releaseAll();
    await this.serviceEngine.close();
    this.configurationPromise = undefined;
    const status = this.querySelector<HTMLOutputElement>("[data-service-status]");
    if (status !== null) status.value = "Not connected";
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
      this.scheduleSettingsSave();
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

  private async startRecording(): Promise<boolean> {
    await this.ensureConfigured();
    return this.engine.action("record-start");
  }

  private async stopTransport(): Promise<void> {
    try {
      await this.ensureConfigured();
      if (!this.recording) {
        await this.engine.action("playback-stop");
        return;
      }
      const recordingStopped = this.waitForEngineEvent(
        (event) => event.type === 7 && event.detail === 0,
        2_000,
      );
      const stoppedRecording = await this.engine.action("record-stop");
      await this.engine.action("playback-stop");
      if (!stoppedRecording) return;
      await recordingStopped;
      const bytes = await this.engine.exportRecording();
      if (this.engine === this.serviceEngine) {
        const storage = this.querySelector<HTMLOutputElement>("[data-recording-storage]");
        if (storage !== null) storage.value = "Desktop service · local .molseq";
        await this.refreshRecordings(this.serviceEngine.lastRemoteRecording);
        return;
      }
      if (bytes === undefined) throw new Error("The engine could not export the recorded sequence");
      const label = this.language === "zh" ? "浏览器录音" : "Browser take";
      const metadata = await saveRecording(bytes, `${label} ${new Date().toLocaleTimeString()}`);
      const storage = this.querySelector<HTMLOutputElement>("[data-recording-storage]");
      if (storage !== null) storage.value = `${metadata.size} B · ${metadata.storage.toUpperCase()}`;
      await this.refreshRecordings(metadata.id);
    } catch (error: unknown) {
      this.setStatus(error instanceof Error ? error.message : "Could not save recording", "error");
    }
  }

  private waitForEngineEvent(
    predicate: (event: EngineEvent) => boolean,
    timeoutMs: number,
  ): Promise<void> {
    return new Promise((resolve, reject) => {
      const timer = window.setTimeout(() => {
        this.engine.removeEventListener("engineevents", listener);
        reject(new Error("Timed out waiting for the audio engine state"));
      }, timeoutMs);
      const listener = (event: Event): void => {
        const events = (event as CustomEvent<readonly EngineEvent[]>).detail;
        if (!events.some(predicate)) return;
        window.clearTimeout(timer);
        this.engine.removeEventListener("engineevents", listener);
        resolve();
      };
      this.engine.addEventListener("engineevents", listener);
    });
  }

  private async playRecording(): Promise<boolean> {
    await this.ensureConfigured();
    return this.engine.action("playback-start");
  }

  private async loadSelectedRecording(): Promise<void> {
    const select = this.querySelector<HTMLSelectElement>("[data-recording-list]");
    if (select === null || select.value === "") return;
    try {
      await this.ensureConfigured();
      const accepted =
        this.engine === this.serviceEngine
          ? await this.serviceEngine.loadRemoteRecording(select.value)
          : await this.engine.loadRecording(await loadStoredRecording(select.value));
      if (!accepted) throw new Error("The audio engine rejected the saved sequence");
      this.setRecordingState("recorded");
    } catch (error: unknown) {
      this.setStatus(error instanceof Error ? error.message : "Could not load recording", "error");
    }
  }

  private async refreshRecordings(selectedId?: string): Promise<void> {
    const select = this.querySelector<HTMLSelectElement>("[data-recording-list]");
    if (select === null) return;
    try {
      if (this.engine === this.serviceEngine) {
        select.replaceChildren(new Option("—", ""));
        if (this.serviceEngine.connected) {
          for (const name of await this.serviceEngine.listRemoteRecordings()) {
            select.add(new Option(name, name));
          }
          if (selectedId !== undefined) select.value = selectedId;
        }
        return;
      }
      const recordings = await listRecordings();
      select.replaceChildren(new Option("—", ""));
      for (const recording of recordings) {
        const option = new Option(`${recording.name} · ${recording.size} B`, recording.id);
        select.add(option);
      }
      if (selectedId !== undefined) select.value = selectedId;
    } catch (error: unknown) {
      const storage = this.querySelector<HTMLOutputElement>("[data-recording-storage]");
      if (storage !== null) {
        storage.value = error instanceof Error ? error.message : "Storage unavailable";
      }
    }
  }

  private async restoreSettings(): Promise<void> {
    const result = await loadSettings();
    const settings = result.settings;
    this.setControlValue("preset", settings.preset);
    this.setControlValue("scale", settings.scale);
    this.setControlValue("tonic", settings.tonic);
    this.setControlValue("octave", settings.octave);
    this.setControlValue("volume", settings.volume);
    this.setControlValue("chord", settings.chord);
    this.setControlValue("arpeggiator", settings.arpeggiator);
    this.setControlValue("arp-rate", settings.arpeggiatorRate);
    this.setControlValue("gate", settings.arpeggiatorGate);
    this.setControlValue("arp-octaves", settings.arpeggiatorOctaves);
    this.setControlValue("tempo", settings.tempo);
    this.setControlValue("time-signature", settings.timeSignature);
    this.setControlValue("portamento", settings.portamento);
    this.setControlValue("portamento-time", settings.portamentoTime);
    this.setControlValue("chorus", settings.chorus);
    this.setControlValue("delay", settings.delay);
    this.setControlValue("reverb", settings.reverb);
    const metronome = this.querySelector<HTMLInputElement>("[data-control='metronome']");
    if (metronome !== null) metronome.checked = settings.metronome;
    const backend = this.querySelector<HTMLSelectElement>("[data-backend]");
    if (backend !== null) backend.value = settings.backend;
    await this.selectBackend(settings.backend);
    this.setMode(settings.mode === "studio");
    this.applyLanguage(settings.language);
    for (const control of ["volume", "chorus", "delay", "reverb", "gate", "portamento-time"]) {
      this.updateRangeOutput(control);
    }
    const storage = this.querySelector<HTMLElement>("[data-storage-state]");
    if (storage !== null) storage.textContent = result.storage === "indexeddb" ? "IndexedDB ready" : "Unavailable";
    if (result.diagnostic !== undefined) this.setStatus(result.diagnostic, "error");
  }

  private setControlValue(name: string, value: string | number): void {
    const control = this.control(name);
    if (control !== null) control.value = String(value);
  }

  private updateRangeOutput(control: string): void {
    const value = this.controlNumber(control);
    const output = this.querySelector<HTMLOutputElement>(`[data-output='${control}']`);
    if (output !== null) {
      output.value = control === "portamento-time" ? `${value} ms` : `${Math.round(value * 100)}%`;
    }
  }

  private collectSettings(): WebSettings {
    const mode = this.querySelector<HTMLButtonElement>("[data-mode='studio']")?.getAttribute("aria-pressed");
    const backendValue = this.querySelector<HTMLSelectElement>("[data-backend]")?.value;
    const timeSignatureValue = this.control("time-signature")?.value;
    return {
      version: 1,
      language: this.language,
      mode: mode === "true" ? "studio" : "explore",
      backend: backendValue === "service" || backendValue === "esp32" ? backendValue : "standalone",
      preset: this.controlNumber("preset"),
      scale: this.controlNumber("scale"),
      tonic: this.controlNumber("tonic"),
      octave: this.controlNumber("octave"),
      volume: this.controlNumber("volume"),
      metronome: this.querySelector<HTMLInputElement>("[data-control='metronome']")?.checked === true,
      chord: this.controlNumber("chord"),
      arpeggiator: this.controlNumber("arpeggiator"),
      arpeggiatorRate: this.controlNumber("arp-rate"),
      arpeggiatorGate: this.controlNumber("gate"),
      arpeggiatorOctaves: this.controlNumber("arp-octaves"),
      tempo: this.controlNumber("tempo"),
      timeSignature:
        timeSignatureValue === "3/4" || timeSignatureValue === "5/4" || timeSignatureValue === "6/8"
          ? timeSignatureValue
          : "4/4",
      portamento: this.controlNumber("portamento"),
      portamentoTime: this.controlNumber("portamento-time"),
      chorus: this.controlNumber("chorus"),
      delay: this.controlNumber("delay"),
      reverb: this.controlNumber("reverb"),
    };
  }

  private scheduleSettingsSave(): void {
    if (this.saveSettingsTimer !== undefined) window.clearTimeout(this.saveSettingsTimer);
    this.saveSettingsTimer = window.setTimeout(() => {
      void saveSettings(this.collectSettings()).catch((error: unknown) => {
        const storage = this.querySelector<HTMLElement>("[data-storage-state]");
        if (storage !== null) storage.textContent = error instanceof Error ? error.message : "Save failed";
      });
    }, 180);
  }

  private async ensureConfigured(): Promise<void> {
    await this.settingsReady;
    await this.engine.start();
    this.configurationPromise ??= this.applyCurrentSettings();
    await this.configurationPromise;
  }

  private async applyCurrentSettings(): Promise<void> {
    const [numerator = 4, denominator = 4] = (this.control("time-signature")?.value ?? "4/4")
      .split("/")
      .map(Number);
    const results = await Promise.all([
      this.engine.setPreset(this.controlNumber("preset")),
      this.engine.setScale(this.controlNumber("scale"), this.controlNumber("tonic"), 0),
      this.engine.setOctave(this.controlNumber("octave")),
      this.engine.setMasterGain(this.controlNumber("volume")),
      this.engine.setMetronome(
        this.querySelector<HTMLInputElement>("[data-control='metronome']")?.checked === true,
        0.5,
      ),
      this.engine.setChord(this.controlNumber("chord")),
      this.engine.setArpeggiator(
        this.controlNumber("arpeggiator"),
        this.controlNumber("arp-rate"),
        this.controlNumber("gate"),
        this.controlNumber("arp-octaves"),
        0x4d4f4c,
      ),
      this.engine.setTempo(this.controlNumber("tempo")),
      this.engine.setTimeSignature(numerator, denominator),
      this.engine.setPortamento(
        this.controlNumber("portamento"),
        this.controlNumber("portamento-time"),
      ),
      this.engine.setParameter(3, this.controlNumber("chorus")),
      this.engine.setParameter(6, this.controlNumber("delay")),
      this.engine.setParameter(11, this.controlNumber("reverb")),
    ]);
    if (results.some((accepted) => !accepted)) throw new Error("One or more saved controls were rejected");
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
      fastPath.textContent =
        this.engine.commandTransport === "websocket-jsonrpc"
          ? this.serviceEngine.connected
            ? "Authenticated loopback WebSocket"
            : "WebSocket disconnected"
          : this.engine.state === "idle"
          ? isolated
            ? "SharedArrayBuffer available"
            : "MessagePort baseline"
          : this.engine.commandTransport === "shared-array-buffer"
            ? "SharedArrayBuffer SPSC"
            : "MessagePort baseline";
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
        this.recording = event.detail !== 0;
        this.setRecordingState(event.detail !== 0 ? "recording" : "recorded");
      } else if (event.type === 14) {
        this.setRecordingState(event.detail !== 0 ? "playing" : "recorded");
      }
    }
    const voiceReadout = this.querySelector<HTMLElement>("[data-active-voices]");
    if (voiceReadout !== null) voiceReadout.textContent = String(this.actualVoices.size);
    const eventReadout = this.querySelector<HTMLElement>("[data-event-count]");
    if (eventReadout !== null) eventReadout.textContent = String(this.coreEventCount);
    const droppedReadout = this.querySelector<HTMLElement>("[data-dropped-count]");
    if (droppedReadout !== null) droppedReadout.textContent = String(this.engine.droppedCommandCount);
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
      await this.ensureConfigured();
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
    void this
      .ensureConfigured()
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
void registerPwa();
