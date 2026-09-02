import "./styles.css";
import { MolAudioEngine } from "./audio-engine";
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
          <div><dt>Voices</dt><dd>8 polyphonic</dd></div>
        </dl>
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
  private canvas: HTMLCanvasElement | undefined;
  private context: CanvasRenderingContext2D | undefined;
  private statusElement: HTMLElement | undefined;
  private startButton: HTMLButtonElement | undefined;
  private nextGestureId = 1;
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
      const sampleRate = this.engine.sampleRate;
      const rateLabel = sampleRate === undefined ? "worklet" : `${Math.round(sampleRate / 100) / 10} kHz worklet`;
      this.setStatus(`Audio ready · ${rateLabel}`, "ready");
      if (this.startButton !== undefined) {
        this.startButton.querySelector("span")!.textContent = "Audio ready";
      }
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
        this.setStatus("Audio ready · playing", "ready");
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
    return new Set([
      ...[...this.activeKeys.values()].map((gesture) => gesture.note),
      ...[...this.activePointers.values()].map((gesture) => gesture.note),
      ...[...this.activeAccessible.values()].map((gesture) => gesture.note),
    ]);
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
