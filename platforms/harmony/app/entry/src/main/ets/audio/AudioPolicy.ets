// SPDX-License-Identifier: Apache-2.0

export class HarmonyAudioPolicy {
  userStarted = false;
  foreground = true;
  continuousTaskActive = false;
  transportRunning = false;
  playbackRunning = false;
  metronomeEnabled = false;

  userStartRequested() {
    this.userStarted = true;
  }

  userStartFailed() {
    this.userStarted = false;
  }

  userStopped() {
    this.userStarted = false;
    this.transportRunning = false;
    this.playbackRunning = false;
  }

  enteredForeground() {
    this.foreground = true;
  }

  enteredBackground() {
    this.foreground = false;
  }

  transportToggleAccepted() {
    this.transportRunning = !this.transportRunning;
  }

  playbackStarted() {
    this.playbackRunning = true;
  }

  playbackStopped() {
    this.playbackRunning = false;
  }

  metronomeEnabledChanged() {
    this.metronomeEnabled = !this.metronomeEnabled;
  }

  continuousTaskStarted() {
    this.continuousTaskActive = true;
  }

  continuousTaskStopped() {
    this.continuousTaskActive = false;
  }

  shouldContinueInBackground() {
    return this.userStarted &&
      (this.playbackRunning || (this.metronomeEnabled && this.transportRunning));
  }

  shouldRetainOnDestroy() {
    return this.continuousTaskActive && this.shouldContinueInBackground();
  }

  shouldAttemptRecovery() {
    return this.userStarted && (this.foreground || this.shouldContinueInBackground());
  }
}
