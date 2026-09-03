// SPDX-License-Identifier: Apache-2.0
import { audio } from '@kit.AudioKit';
import { avSession } from '@kit.AVSessionKit';
import { common, wantAgent, WantAgent } from '@kit.AbilityKit';
import { preferences } from '@kit.ArkData';
import { BusinessError } from '@kit.BasicServicesKit';
import { backgroundTaskManager } from '@kit.BackgroundTasksKit';
import { fileIo } from '@kit.CoreFileKit';
import { AudioRuntime, NativeAudioEvent } from './AudioRuntime';
import { NativeCommands, NativeEvents } from './NativeCommands';

const SETTINGS_STORE: string = 'mol_keyboard_settings';
const SEQUENCE_FILE: string = 'last-recording.molseq';
const MAXIMUM_SEQUENCE_BYTES: number = 2 * 1024 * 1024;
const STATUS_INTERVAL_MS: number = 125;

export interface AudioSettings {
  masterGain: number;
  preset: number;
  octave: number;
  transpose: number;
  scale: number;
  tonic: number;
  scaleMapping: number;
  chord: number;
  arpeggiatorMode: number;
  arpeggiatorRate: number;
  arpeggiatorGate: number;
  arpeggiatorOctaves: number;
  tempo: number;
  meterNumerator: number;
  meterDenominator: number;
  metronomeEnabled: boolean;
  metronomeLevel: number;
  portamentoMode: number;
  portamentoTimeMs: number;
}

export interface RuntimeSnapshot {
  initialized: boolean;
  userStarted: boolean;
  active: boolean;
  fastPathActive: boolean;
  latencyFallbackUsed: boolean;
  sampleRate: number;
  frameSize: number;
  callbackCount: number;
  renderedFrames: number;
  renderFailures: number;
  nonFiniteSamples: number;
  underflowCount: number;
  routeChanges: number;
  interruptions: number;
  lastError: number;
  transportRunning: boolean;
  recording: boolean;
  recordingAvailable: boolean;
  playbackRunning: boolean;
  backgroundTaskActive: boolean;
  statusMessage: string;
}

const DEFAULT_SETTINGS: AudioSettings = {
  masterGain: 0.82,
  preset: 0,
  octave: 0,
  transpose: 0,
  scale: 0,
  tonic: 0,
  scaleMapping: 0,
  chord: 0,
  arpeggiatorMode: 0,
  arpeggiatorRate: 4,
  arpeggiatorGate: 0.72,
  arpeggiatorOctaves: 1,
  tempo: 120,
  meterNumerator: 4,
  meterDenominator: 4,
  metronomeEnabled: false,
  metronomeLevel: 0.55,
  portamentoMode: 0,
  portamentoTimeMs: 80,
};

export class AudioService {
  private static instance: AudioService | undefined = undefined;
  private readonly runtime: AudioRuntime = new AudioRuntime();
  private context: common.UIAbilityContext | undefined = undefined;
  private settingStore: preferences.Preferences | undefined = undefined;
  private audioSessionManager: audio.AudioSessionManager | undefined = undefined;
  private mediaSession: avSession.AVSession | undefined = undefined;
  private settings: AudioSettings = {
    masterGain: DEFAULT_SETTINGS.masterGain,
    preset: DEFAULT_SETTINGS.preset,
    octave: DEFAULT_SETTINGS.octave,
    transpose: DEFAULT_SETTINGS.transpose,
    scale: DEFAULT_SETTINGS.scale,
    tonic: DEFAULT_SETTINGS.tonic,
    scaleMapping: DEFAULT_SETTINGS.scaleMapping,
    chord: DEFAULT_SETTINGS.chord,
    arpeggiatorMode: DEFAULT_SETTINGS.arpeggiatorMode,
    arpeggiatorRate: DEFAULT_SETTINGS.arpeggiatorRate,
    arpeggiatorGate: DEFAULT_SETTINGS.arpeggiatorGate,
    arpeggiatorOctaves: DEFAULT_SETTINGS.arpeggiatorOctaves,
    tempo: DEFAULT_SETTINGS.tempo,
    meterNumerator: DEFAULT_SETTINGS.meterNumerator,
    meterDenominator: DEFAULT_SETTINGS.meterDenominator,
    metronomeEnabled: DEFAULT_SETTINGS.metronomeEnabled,
    metronomeLevel: DEFAULT_SETTINGS.metronomeLevel,
    portamentoMode: DEFAULT_SETTINGS.portamentoMode,
    portamentoTimeMs: DEFAULT_SETTINGS.portamentoTimeMs,
  };
  private initialized: boolean = false;
  private userStarted: boolean = false;
  private foreground: boolean = true;
  private focusActive: boolean = false;
  private continuousTaskActive: boolean = false;
  private transportRunning: boolean = false;
  private recording: boolean = false;
  private recordingAvailable: boolean = false;
  private playbackRunning: boolean = false;
  private statusMessage: string = 'Tap Start Audio to begin';
  private timerId: number = -1;
  private tickInProgress: boolean = false;

  private readonly onAudioSessionDeactivated =
    (event: audio.AudioSessionDeactivatedEvent): void => {
      console.warn(`MoL audio session deactivated: ${event.reason}`);
      this.focusActive = false;
      if (this.runtime.status().active) {
        this.runtime.submitControl(NativeCommands.ALL_SOUND_OFF);
        this.runtime.stop();
      }
      this.statusMessage = 'Audio focus lost; return to the app to resume';
      void this.stopContinuousTask();
    };

  private readonly onSystemPlay = (): void => {
    void this.startPlayback();
  };

  private readonly onSystemPause = (): void => {
    this.stopPlayback();
  };

  static getInstance(): AudioService {
    if (AudioService.instance === undefined) {
      AudioService.instance = new AudioService();
    }
    return AudioService.instance;
  }

  async initialize(context: common.UIAbilityContext): Promise<void> {
    if (this.initialized) {
      this.context = context;
      return;
    }
    this.context = context;
    this.settingStore = preferences.getPreferencesSync(context, { name: SETTINGS_STORE });
    this.settings = this.readSettings();
    this.audioSessionManager = audio.getAudioManager().getSessionManager();
    this.audioSessionManager.on('audioSessionDeactivated', this.onAudioSessionDeactivated);
    this.recordingAvailable = this.hasPersistedSequence();
    this.timerId = setInterval(() => {
      void this.tick();
    }, STATUS_INTERVAL_MS);
    this.initialized = true;
  }

  async startByUser(): Promise<number> {
    this.userStarted = true;
    const result: number = await this.startNativeAudio();
    if (result !== 0) {
      this.userStarted = false;
    }
    return result;
  }

  async stopByUser(): Promise<void> {
    this.userStarted = false;
    this.transportRunning = false;
    this.playbackRunning = false;
    this.recording = false;
    if (this.runtime.status().active) {
      this.runtime.submitControl(NativeCommands.ALL_SOUND_OFF);
      this.runtime.stop();
    }
    await this.stopContinuousTask();
    await this.deactivateAudioFocus();
    this.updateMediaPlaybackState(false);
    this.statusMessage = 'Audio stopped';
  }

  async onForeground(): Promise<void> {
    this.foreground = true;
    await this.stopContinuousTask();
    if (this.userStarted && !this.runtime.status().active) {
      await this.startNativeAudio();
    }
  }

  async onBackground(): Promise<void> {
    this.foreground = false;
    if (this.runtime.status().active) {
      this.runtime.submitControl(NativeCommands.ALL_NOTES_OFF);
    }
    await this.reconcileBackgroundPolicy();
  }

  async onAbilityDestroy(): Promise<void> {
    if (this.continuousTaskActive && this.shouldContinueInBackground()) {
      return;
    }
    await this.stopByUser();
    this.releasePlatformListeners();
  }

  noteOn(note: number, velocity: number, gestureId: number): boolean {
    if (!this.runtime.status().active || note < 0 || note > 127 ||
      velocity < 0 || velocity > 1 || gestureId < 0) {
      return false;
    }
    return this.runtime.noteOn(note, velocity, gestureId) === 0;
  }

  noteOff(note: number, gestureId: number): boolean {
    if (!this.runtime.status().active || note < 0 || note > 127 || gestureId < 0) {
      return false;
    }
    return this.runtime.noteOff(note, gestureId) === 0;
  }

  setSustain(enabled: boolean): boolean {
    return this.submitIfActive(NativeCommands.SUSTAIN, 0, 0, 0, 0, enabled ? 1 : 0);
  }

  setMasterGain(value: number): boolean {
    if (!this.inRange(value, 0, 1)) return false;
    this.settings.masterGain = value;
    this.persistNumber('masterGain', value);
    return this.submitIfActive(NativeCommands.SET_MASTER_GAIN, 0, 0, 0, 0, value);
  }

  setPreset(value: number): boolean {
    if (!this.integerInRange(value, 0, 17)) return false;
    this.settings.preset = value;
    this.persistNumber('preset', value);
    return this.submitIfActive(NativeCommands.SET_PRESET, value, 0);
  }

  setOctave(value: number): boolean {
    if (!this.integerInRange(value, -2, 2)) return false;
    this.settings.octave = value;
    this.persistNumber('octave', value);
    return this.submitIfActive(NativeCommands.SET_OCTAVE, value);
  }

  setTranspose(value: number): boolean {
    if (!this.integerInRange(value, -12, 12)) return false;
    this.settings.transpose = value;
    this.persistNumber('transpose', value);
    return this.submitIfActive(NativeCommands.SET_TRANSPOSE, value);
  }

  setScale(type: number, tonic: number, mapping: number): boolean {
    if (!this.integerInRange(type, 0, 7) || !this.integerInRange(tonic, 0, 11) ||
      !this.integerInRange(mapping, 0, 2)) {
      return false;
    }
    this.settings.scale = type;
    this.settings.tonic = tonic;
    this.settings.scaleMapping = mapping;
    this.persistNumber('scale', type);
    this.persistNumber('tonic', tonic);
    this.persistNumber('scaleMapping', mapping);
    return this.submitIfActive(NativeCommands.SET_SCALE, type, tonic, mapping);
  }

  setChord(value: number): boolean {
    if (!this.integerInRange(value, 0, 9)) return false;
    this.settings.chord = value;
    this.persistNumber('chord', value);
    return this.submitIfActive(NativeCommands.SET_CHORD, value);
  }

  setArpeggiator(mode: number, rate: number, gate: number, octaves: number): boolean {
    if (!this.integerInRange(mode, 0, 6) || !this.integerInRange(rate, 0, 5) ||
      !this.inRange(gate, 0.05, 1) || !this.integerInRange(octaves, 1, 4)) {
      return false;
    }
    this.settings.arpeggiatorMode = mode;
    this.settings.arpeggiatorRate = rate;
    this.settings.arpeggiatorGate = gate;
    this.settings.arpeggiatorOctaves = octaves;
    this.persistNumber('arpeggiatorMode', mode);
    this.persistNumber('arpeggiatorRate', rate);
    this.persistNumber('arpeggiatorGate', gate);
    this.persistNumber('arpeggiatorOctaves', octaves);
    return this.submitIfActive(NativeCommands.SET_ARPEGGIATOR,
      mode, rate, octaves, 0, gate);
  }

  setTempo(value: number): boolean {
    if (!this.inRange(value, 30, 300)) return false;
    this.settings.tempo = value;
    this.persistNumber('tempo', value);
    return this.submitIfActive(NativeCommands.SET_TEMPO, 0, 0, 0, 0, value);
  }

  setTimeSignature(numerator: number, denominator: number): boolean {
    if (!this.integerInRange(numerator, 1, 16) ||
      ![2, 4, 8, 16].includes(denominator)) {
      return false;
    }
    this.settings.meterNumerator = numerator;
    this.settings.meterDenominator = denominator;
    this.persistNumber('meterNumerator', numerator);
    this.persistNumber('meterDenominator', denominator);
    return this.submitIfActive(NativeCommands.SET_TIME_SIGNATURE, numerator, denominator);
  }

  setMetronome(enabled: boolean, level: number): boolean {
    if (!this.inRange(level, 0, 1)) return false;
    this.settings.metronomeEnabled = enabled;
    this.settings.metronomeLevel = level;
    this.persistBoolean('metronomeEnabled', enabled);
    this.persistNumber('metronomeLevel', level);
    const accepted: boolean = this.submitIfActive(NativeCommands.SET_METRONOME,
      enabled ? 1 : 0, 0, 0, 0, level);
    void this.reconcileBackgroundPolicy();
    return accepted;
  }

  setPortamento(mode: number, timeMs: number): boolean {
    if (!this.integerInRange(mode, 0, 2) || !this.inRange(timeMs, 0, 2000)) return false;
    this.settings.portamentoMode = mode;
    this.settings.portamentoTimeMs = timeMs;
    this.persistNumber('portamentoMode', mode);
    this.persistNumber('portamentoTimeMs', timeMs);
    return this.submitIfActive(NativeCommands.SET_PORTAMENTO, mode, 0, 0, 0, timeMs);
  }

  toggleTransport(): boolean {
    const next: boolean = !this.transportRunning;
    const accepted: boolean = this.submitIfActive(next ? NativeCommands.TRANSPORT_START :
      NativeCommands.TRANSPORT_STOP);
    if (accepted) this.transportRunning = next;
    void this.reconcileBackgroundPolicy();
    return accepted;
  }

  toggleRecording(): boolean {
    const next: boolean = !this.recording;
    const accepted: boolean = this.submitIfActive(next ? NativeCommands.RECORD_START :
      NativeCommands.RECORD_STOP);
    if (accepted) this.recording = next;
    return accepted;
  }

  async startPlayback(): Promise<boolean> {
    this.userStarted = true;
    if (!this.runtime.status().active && await this.startNativeAudio() !== 0) {
      this.userStarted = false;
      return false;
    }
    const accepted: boolean = this.submitIfActive(NativeCommands.PLAYBACK_START);
    if (accepted) {
      this.playbackRunning = true;
      this.updateMediaPlaybackState(true);
      await this.reconcileBackgroundPolicy();
    }
    return accepted;
  }

  stopPlayback(): boolean {
    const accepted: boolean = this.submitIfActive(NativeCommands.PLAYBACK_STOP);
    if (accepted) {
      this.playbackRunning = false;
      this.updateMediaPlaybackState(false);
      void this.reconcileBackgroundPolicy();
    }
    return accepted;
  }

  getSettings(): AudioSettings {
    return {
      masterGain: this.settings.masterGain,
      preset: this.settings.preset,
      octave: this.settings.octave,
      transpose: this.settings.transpose,
      scale: this.settings.scale,
      tonic: this.settings.tonic,
      scaleMapping: this.settings.scaleMapping,
      chord: this.settings.chord,
      arpeggiatorMode: this.settings.arpeggiatorMode,
      arpeggiatorRate: this.settings.arpeggiatorRate,
      arpeggiatorGate: this.settings.arpeggiatorGate,
      arpeggiatorOctaves: this.settings.arpeggiatorOctaves,
      tempo: this.settings.tempo,
      meterNumerator: this.settings.meterNumerator,
      meterDenominator: this.settings.meterDenominator,
      metronomeEnabled: this.settings.metronomeEnabled,
      metronomeLevel: this.settings.metronomeLevel,
      portamentoMode: this.settings.portamentoMode,
      portamentoTimeMs: this.settings.portamentoTimeMs,
    };
  }

  getSnapshot(): RuntimeSnapshot {
    const nativeStatus = this.runtime.status();
    return {
      initialized: this.initialized,
      userStarted: this.userStarted,
      active: nativeStatus.active,
      fastPathActive: nativeStatus.fastPathActive,
      latencyFallbackUsed: nativeStatus.latencyFallbackUsed,
      sampleRate: nativeStatus.sampleRate,
      frameSize: nativeStatus.frameSize,
      callbackCount: nativeStatus.callbackCount,
      renderedFrames: nativeStatus.renderedFrames,
      renderFailures: nativeStatus.renderFailures,
      nonFiniteSamples: nativeStatus.nonFiniteSamples,
      underflowCount: nativeStatus.underflowCount,
      routeChanges: nativeStatus.routeChanges,
      interruptions: nativeStatus.interruptions,
      lastError: nativeStatus.lastError,
      transportRunning: this.transportRunning,
      recording: this.recording,
      recordingAvailable: this.recordingAvailable,
      playbackRunning: this.playbackRunning,
      backgroundTaskActive: this.continuousTaskActive,
      statusMessage: this.statusMessage,
    };
  }
  private async startNativeAudio(): Promise<number> {
    if (!this.initialized || this.context === undefined ||
      this.audioSessionManager === undefined) {
      this.statusMessage = 'Audio service is not initialized';
      return -1;
    }
    if (this.runtime.status().active) return 0;
    if (!await this.activateAudioFocus()) return -1;
    const result: number = this.runtime.start();
    if (result !== 0) {
      await this.deactivateAudioFocus();
      this.statusMessage = `OHAudio start failed (${result})`;
      return result;
    }
    this.applyPersistentControls();
    this.loadPersistedSequence();
    this.restoreTransportState();
    await this.ensureMediaSession();
    const status = this.runtime.status();
    this.statusMessage = status.fastPathActive ? 'OHAudio fast path active' :
      'OHAudio normal path active';
    return 0;
  }

  private async activateAudioFocus(): Promise<boolean> {
    if (this.audioSessionManager === undefined) return false;
    if (this.audioSessionManager.isAudioSessionActivated()) {
      this.focusActive = true;
      return true;
    }
    const strategy: audio.AudioSessionStrategy = {
      concurrencyMode: audio.AudioConcurrencyMode.CONCURRENCY_DEFAULT,
    };
    try {
      await this.audioSessionManager.activateAudioSession(strategy);
      this.focusActive = true;
      return true;
    } catch (error) {
      const failure: BusinessError = error as BusinessError;
      console.error(`MoL audio focus activation failed: ${failure.code} ${failure.message}`);
      this.statusMessage = `Audio focus unavailable (${failure.code})`;
      return false;
    }
  }

  private async deactivateAudioFocus(): Promise<void> {
    if (!this.focusActive || this.audioSessionManager === undefined) return;
    try {
      await this.audioSessionManager.deactivateAudioSession();
    } catch (error) {
      const failure: BusinessError = error as BusinessError;
      console.error(`MoL audio focus release failed: ${failure.code} ${failure.message}`);
    }
    this.focusActive = false;
  }

  private applyPersistentControls(): void {
    this.runtime.submitControl(NativeCommands.SET_MASTER_GAIN, 0, 0, 0, 0, 0,
      this.settings.masterGain, 0);
    this.runtime.submitControl(NativeCommands.SET_PRESET, 0, this.settings.preset);
    this.runtime.submitControl(NativeCommands.SET_OCTAVE, 0, this.settings.octave);
    this.runtime.submitControl(NativeCommands.SET_TRANSPOSE, 0, this.settings.transpose);
    this.runtime.submitControl(NativeCommands.SET_SCALE, 0, this.settings.scale,
      this.settings.tonic, this.settings.scaleMapping);
    this.runtime.submitControl(NativeCommands.SET_CHORD, 0, this.settings.chord);
    this.runtime.submitControl(NativeCommands.SET_ARPEGGIATOR, 0,
      this.settings.arpeggiatorMode, this.settings.arpeggiatorRate,
      this.settings.arpeggiatorOctaves, 0, this.settings.arpeggiatorGate, 0);
    this.runtime.submitControl(NativeCommands.SET_TEMPO, 0, 0, 0, 0, 0,
      this.settings.tempo, 0);
    this.runtime.submitControl(NativeCommands.SET_TIME_SIGNATURE, 0,
      this.settings.meterNumerator, this.settings.meterDenominator);
    this.runtime.submitControl(NativeCommands.SET_METRONOME, 0,
      this.settings.metronomeEnabled ? 1 : 0, 0, 0, 0,
      this.settings.metronomeLevel, 0);
    this.runtime.submitControl(NativeCommands.SET_PORTAMENTO, 0,
      this.settings.portamentoMode, 0, 0, 0, this.settings.portamentoTimeMs, 0);
  }

  private restoreTransportState(): void {
    if (this.transportRunning) {
      this.runtime.submitControl(NativeCommands.TRANSPORT_START);
    }
    if (this.playbackRunning) {
      this.runtime.submitControl(NativeCommands.PLAYBACK_START);
    }
  }

  private submitIfActive(commandType: number, integer0: number = 0,
    integer1: number = 0, integer2: number = 0, integer3: number = 0,
    scalar0: number = 0, scalar1: number = 0): boolean {
    if (!this.runtime.status().active) return true;
    return this.runtime.submitControl(commandType, 0, integer0, integer1,
      integer2, integer3, scalar0, scalar1) === 0;
  }

  private async tick(): Promise<void> {
    if (this.tickInProgress || !this.initialized) return;
    this.tickInProgress = true;
    try {
      const status = this.runtime.status();
      if (status.needsRestart && this.userStarted &&
        (this.foreground || this.shouldContinueInBackground())) {
        const result: number = this.runtime.recover();
        if (result === 0) {
          this.applyPersistentControls();
          this.loadPersistedSequence();
          this.restoreTransportState();
          this.statusMessage = this.runtime.status().fastPathActive ?
            'Audio route restored on fast path' : 'Audio route restored on normal path';
        } else {
          this.statusMessage = `Audio route recovery failed (${result})`;
        }
      }
      const events: NativeAudioEvent[] = this.runtime.pollEvents();
      for (let index: number = 0; index < events.length; ++index) {
        this.handleNativeEvent(events[index]);
      }
    } finally {
      this.tickInProgress = false;
    }
  }

  private handleNativeEvent(event: NativeAudioEvent): void {
    if (event.eventType === NativeEvents.RECORDING_CHANGED) {
      this.recording = event.value !== 0;
      if (!this.recording) {
        this.recordingAvailable = this.saveRecording();
      }
    } else if (event.eventType === NativeEvents.PLAYBACK_CHANGED) {
      this.playbackRunning = event.value !== 0;
      this.updateMediaPlaybackState(this.playbackRunning);
      void this.reconcileBackgroundPolicy();
    }
  }

  private shouldContinueInBackground(): boolean {
    return this.userStarted && (this.playbackRunning ||
      (this.settings.metronomeEnabled && this.transportRunning));
  }

  private async reconcileBackgroundPolicy(): Promise<void> {
    if (this.foreground) {
      await this.stopContinuousTask();
      return;
    }
    if (this.shouldContinueInBackground()) {
      if (!this.runtime.status().active && await this.startNativeAudio() !== 0) return;
      await this.startContinuousTask();
      return;
    }
    await this.stopContinuousTask();
    if (this.runtime.status().active) {
      this.runtime.submitControl(NativeCommands.ALL_SOUND_OFF);
      this.runtime.stop();
    }
    await this.deactivateAudioFocus();
    this.statusMessage = 'Idle background audio released';
  }

  private async startContinuousTask(): Promise<void> {
    if (this.continuousTaskActive || this.context === undefined) return;
    const context: common.UIAbilityContext = this.context;
    const agentInfo: wantAgent.WantAgentInfo = {
      wants: [{
        bundleName: context.abilityInfo.bundleName,
        abilityName: context.abilityInfo.name,
      }],
      operationType: wantAgent.OperationType.START_ABILITY,
      requestCode: 0,
      wantAgentFlags: [wantAgent.WantAgentFlags.UPDATE_PRESENT_FLAG],
    };
    try {
      const agent: WantAgent = await wantAgent.getWantAgent(agentInfo);
      await backgroundTaskManager.startBackgroundRunning(context,
        backgroundTaskManager.BackgroundMode.AUDIO_PLAYBACK, agent);
      this.continuousTaskActive = true;
      this.statusMessage = 'Official audio playback continuous task active';
    } catch (error) {
      const failure: BusinessError = error as BusinessError;
      console.error(`MoL continuous task start failed: ${failure.code} ${failure.message}`);
      this.statusMessage = `Background task unavailable (${failure.code})`;
      if (this.runtime.status().active) this.runtime.stop();
      await this.deactivateAudioFocus();
    }
  }

  private async stopContinuousTask(): Promise<void> {
    if (!this.continuousTaskActive || this.context === undefined) return;
    try {
      await backgroundTaskManager.stopBackgroundRunning(this.context);
    } catch (error) {
      const failure: BusinessError = error as BusinessError;
      console.error(`MoL continuous task stop failed: ${failure.code} ${failure.message}`);
    }
    this.continuousTaskActive = false;
  }

  private async ensureMediaSession(): Promise<void> {
    if (this.context === undefined) return;
    if (this.mediaSession === undefined) {
      try {
        this.mediaSession = await avSession.createAVSession(this.context,
          'MOL_KEYBOARD_AUDIO', 'audio');
        this.mediaSession.on('play', this.onSystemPlay);
        this.mediaSession.on('pause', this.onSystemPause);
        const agentInfo: wantAgent.WantAgentInfo = {
          wants: [{
            bundleName: this.context.abilityInfo.bundleName,
            abilityName: this.context.abilityInfo.name,
          }],
          operationType: wantAgent.OperationType.START_ABILITY,
          requestCode: 1,
          wantAgentFlags: [wantAgent.WantAgentFlags.UPDATE_PRESENT_FLAG],
        };
        const agent: WantAgent = await wantAgent.getWantAgent(agentInfo);
        await this.mediaSession.setLaunchAbility(agent);
        await this.mediaSession.setAVMetadata({
          assetId: 'mol-keyboard-sequence',
          title: 'MoL Keyboard Sequence',
          artist: 'MoL Keyboard',
        });
      } catch (error) {
        const failure: BusinessError = error as BusinessError;
        console.error(`MoL AVSession creation failed: ${failure.code} ${failure.message}`);
        this.mediaSession = undefined;
        return;
      }
    }
    try {
      await this.mediaSession.activate();
    } catch (error) {
      const failure: BusinessError = error as BusinessError;
      console.error(`MoL AVSession activation failed: ${failure.code} ${failure.message}`);
    }
  }

  private updateMediaPlaybackState(playing: boolean): void {
    if (this.mediaSession === undefined) return;
    this.mediaSession.setAVPlaybackState({
      state: playing ? avSession.PlaybackState.PLAYBACK_STATE_PLAY :
        avSession.PlaybackState.PLAYBACK_STATE_PAUSE,
    }, (error: BusinessError): void => {
      if (error) {
        console.error(`MoL AVSession state failed: ${error.code} ${error.message}`);
      }
    });
  }

  private releasePlatformListeners(): void {
    if (this.timerId >= 0) {
      clearInterval(this.timerId);
      this.timerId = -1;
    }
    if (this.audioSessionManager !== undefined) {
      this.audioSessionManager.off('audioSessionDeactivated', this.onAudioSessionDeactivated);
    }
    if (this.mediaSession !== undefined) {
      this.mediaSession.off('play', this.onSystemPlay);
      this.mediaSession.off('pause', this.onSystemPause);
      void this.mediaSession.destroy();
      this.mediaSession = undefined;
    }
    this.initialized = false;
  }

  private sequencePath(): string {
    return `${this.context?.filesDir}/${SEQUENCE_FILE}`;
  }

  private hasPersistedSequence(): boolean {
    if (this.context === undefined) return false;
    try {
      const size: number = fileIo.statSync(this.sequencePath()).size;
      return size > 0 && size <= MAXIMUM_SEQUENCE_BYTES;
    } catch (_) {
      return false;
    }
  }

  private loadPersistedSequence(): void {
    if (!this.hasPersistedSequence()) return;
    try {
      const path: string = this.sequencePath();
      const size: number = fileIo.statSync(path).size;
      const recording: ArrayBuffer = new ArrayBuffer(size);
      const file: fileIo.File = fileIo.openSync(path, fileIo.OpenMode.READ_ONLY);
      const read: number = fileIo.readSync(file.fd, recording);
      fileIo.closeSync(file);
      if (read === size && this.runtime.loadRecording(recording) === 0) {
        this.recordingAvailable = true;
      }
    } catch (error) {
      const failure: BusinessError = error as BusinessError;
      console.error(`MoL sequence load failed: ${failure.code} ${failure.message}`);
    }
  }

  private saveRecording(): boolean {
    const recording: ArrayBuffer | number = this.runtime.exportRecording();
    if (typeof recording === 'number' || recording.byteLength === 0 ||
      recording.byteLength > MAXIMUM_SEQUENCE_BYTES || this.context === undefined) {
      return false;
    }
    const path: string = this.sequencePath();
    const temporaryPath: string = `${path}.tmp`;
    try {
      const file: fileIo.File = fileIo.openSync(temporaryPath,
        fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.CREATE | fileIo.OpenMode.TRUNC);
      const written: number = fileIo.writeSync(file.fd, recording);
      fileIo.fsyncSync(file.fd);
      fileIo.closeSync(file);
      if (written !== recording.byteLength) {
        fileIo.unlinkSync(temporaryPath);
        return false;
      }
      fileIo.renameSync(temporaryPath, path);
      return true;
    } catch (error) {
      const failure: BusinessError = error as BusinessError;
      console.error(`MoL sequence save failed: ${failure.code} ${failure.message}`);
      try {
        fileIo.unlinkSync(temporaryPath);
      } catch (_) {
      }
      return false;
    }
  }

  private readSettings(): AudioSettings {
    return {
      masterGain: this.readNumber('masterGain', DEFAULT_SETTINGS.masterGain, 0, 1),
      preset: this.readInteger('preset', DEFAULT_SETTINGS.preset, 0, 17),
      octave: this.readInteger('octave', DEFAULT_SETTINGS.octave, -2, 2),
      transpose: this.readInteger('transpose', DEFAULT_SETTINGS.transpose, -12, 12),
      scale: this.readInteger('scale', DEFAULT_SETTINGS.scale, 0, 7),
      tonic: this.readInteger('tonic', DEFAULT_SETTINGS.tonic, 0, 11),
      scaleMapping: this.readInteger('scaleMapping', DEFAULT_SETTINGS.scaleMapping, 0, 2),
      chord: this.readInteger('chord', DEFAULT_SETTINGS.chord, 0, 9),
      arpeggiatorMode: this.readInteger('arpeggiatorMode',
        DEFAULT_SETTINGS.arpeggiatorMode, 0, 6),
      arpeggiatorRate: this.readInteger('arpeggiatorRate',
        DEFAULT_SETTINGS.arpeggiatorRate, 0, 5),
      arpeggiatorGate: this.readNumber('arpeggiatorGate',
        DEFAULT_SETTINGS.arpeggiatorGate, 0.05, 1),
      arpeggiatorOctaves: this.readInteger('arpeggiatorOctaves',
        DEFAULT_SETTINGS.arpeggiatorOctaves, 1, 4),
      tempo: this.readNumber('tempo', DEFAULT_SETTINGS.tempo, 30, 300),
      meterNumerator: this.readInteger('meterNumerator',
        DEFAULT_SETTINGS.meterNumerator, 1, 16),
      meterDenominator: this.readInteger('meterDenominator',
        DEFAULT_SETTINGS.meterDenominator, 2, 16),
      metronomeEnabled: this.readBoolean('metronomeEnabled',
        DEFAULT_SETTINGS.metronomeEnabled),
      metronomeLevel: this.readNumber('metronomeLevel',
        DEFAULT_SETTINGS.metronomeLevel, 0, 1),
      portamentoMode: this.readInteger('portamentoMode',
        DEFAULT_SETTINGS.portamentoMode, 0, 2),
      portamentoTimeMs: this.readNumber('portamentoTimeMs',
        DEFAULT_SETTINGS.portamentoTimeMs, 0, 2000),
    };
  }

  private readNumber(key: string, fallback: number, minimum: number, maximum: number): number {
    const value: preferences.ValueType | undefined = this.settingStore?.getSync(key, fallback);
    return typeof value === 'number' && this.inRange(value, minimum, maximum) ? value : fallback;
  }

  private readInteger(key: string, fallback: number, minimum: number, maximum: number): number {
    const value: number = this.readNumber(key, fallback, minimum, maximum);
    return Number.isInteger(value) ? value : fallback;
  }

  private readBoolean(key: string, fallback: boolean): boolean {
    const value: preferences.ValueType | undefined = this.settingStore?.getSync(key, fallback);
    return typeof value === 'boolean' ? value : fallback;
  }

  private persistNumber(key: string, value: number): void {
    this.settingStore?.putSync(key, value);
    void this.settingStore?.flush();
  }

  private persistBoolean(key: string, value: boolean): void {
    this.settingStore?.putSync(key, value);
    void this.settingStore?.flush();
  }

  private inRange(value: number, minimum: number, maximum: number): boolean {
    return Number.isFinite(value) && value >= minimum && value <= maximum;
  }

  private integerInRange(value: number, minimum: number, maximum: number): boolean {
    return Number.isInteger(value) && value >= minimum && value <= maximum;
  }
}
