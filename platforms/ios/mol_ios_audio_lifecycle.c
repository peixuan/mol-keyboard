// SPDX-License-Identifier: Apache-2.0
#include "mol_ios_audio_lifecycle.h"

#include <limits.h>
#include <stddef.h>

#include "mol/command.h"

static void reset_playback_state(mol_ios_audio_lifecycle_t* lifecycle) {
  lifecycle->transport_running = false;
  lifecycle->playback_running = false;
  lifecycle->metronome_enabled = false;
}

static void increment_route_revision(mol_ios_audio_lifecycle_t* lifecycle) {
  if (lifecycle->route_revision != UINT64_MAX) {
    lifecycle->route_revision += 1u;
  }
}

static uint32_t stop_if_idle_in_background(mol_ios_audio_lifecycle_t* lifecycle) {
  if (lifecycle->ui_foreground || mol_ios_audio_lifecycle_allows_background(lifecycle)) {
    return MOL_IOS_AUDIO_ACTION_NONE;
  }
  mol_ios_audio_lifecycle_stop(lifecycle);
  return MOL_IOS_AUDIO_ACTION_STOP;
}

void mol_ios_audio_lifecycle_init(mol_ios_audio_lifecycle_t* lifecycle) {
  if (lifecycle == NULL) {
    return;
  }
  *lifecycle = (mol_ios_audio_lifecycle_t){0};
  lifecycle->ui_foreground = true;
}

uint32_t mol_ios_audio_lifecycle_start_succeeded(mol_ios_audio_lifecycle_t* lifecycle) {
  if (lifecycle == NULL) {
    return MOL_IOS_AUDIO_ACTION_NONE;
  }
  lifecycle->user_started = true;
  return MOL_IOS_AUDIO_ACTION_RESTORE_STATE;
}

void mol_ios_audio_lifecycle_start_failed(mol_ios_audio_lifecycle_t* lifecycle) {
  if (lifecycle != NULL) {
    lifecycle->user_started = false;
  }
}

void mol_ios_audio_lifecycle_stop(mol_ios_audio_lifecycle_t* lifecycle) {
  if (lifecycle == NULL) {
    return;
  }
  lifecycle->user_started = false;
  reset_playback_state(lifecycle);
}

void mol_ios_audio_lifecycle_did_become_active(mol_ios_audio_lifecycle_t* lifecycle) {
  if (lifecycle != NULL) {
    lifecycle->ui_foreground = true;
  }
}

uint32_t mol_ios_audio_lifecycle_will_resign_active(mol_ios_audio_lifecycle_t* lifecycle,
                                                    bool host_active) {
  if (lifecycle == NULL) {
    return MOL_IOS_AUDIO_ACTION_NONE;
  }
  lifecycle->ui_foreground = false;
  return host_active ? MOL_IOS_AUDIO_ACTION_ALL_NOTES_OFF : MOL_IOS_AUDIO_ACTION_NONE;
}

uint32_t mol_ios_audio_lifecycle_did_enter_background(mol_ios_audio_lifecycle_t* lifecycle,
                                                      bool host_active) {
  uint32_t actions;
  if (lifecycle == NULL) {
    return MOL_IOS_AUDIO_ACTION_NONE;
  }
  lifecycle->ui_foreground = false;
  actions = host_active ? MOL_IOS_AUDIO_ACTION_ALL_NOTES_OFF : MOL_IOS_AUDIO_ACTION_NONE;
  return actions | stop_if_idle_in_background(lifecycle);
}

uint32_t mol_ios_audio_lifecycle_command_submitted(mol_ios_audio_lifecycle_t* lifecycle,
                                                   uint32_t command_type, int32_t integer_0) {
  if (lifecycle == NULL) {
    return MOL_IOS_AUDIO_ACTION_NONE;
  }
  switch (command_type) {
    case MOL_COMMAND_TRANSPORT_START:
      lifecycle->transport_running = true;
      break;
    case MOL_COMMAND_TRANSPORT_STOP:
      lifecycle->transport_running = false;
      break;
    case MOL_COMMAND_PLAYBACK_START:
      lifecycle->playback_running = true;
      break;
    case MOL_COMMAND_PLAYBACK_STOP:
      lifecycle->playback_running = false;
      break;
    case MOL_COMMAND_SET_METRONOME:
      lifecycle->metronome_enabled = integer_0 != 0;
      break;
    case MOL_COMMAND_RESET_ENGINE:
      reset_playback_state(lifecycle);
      break;
    default:
      return MOL_IOS_AUDIO_ACTION_NONE;
  }
  return stop_if_idle_in_background(lifecycle);
}

uint32_t mol_ios_audio_lifecycle_playback_changed(mol_ios_audio_lifecycle_t* lifecycle,
                                                  bool running) {
  if (lifecycle == NULL) {
    return MOL_IOS_AUDIO_ACTION_NONE;
  }
  lifecycle->playback_running = running;
  return stop_if_idle_in_background(lifecycle);
}

uint32_t mol_ios_audio_lifecycle_host_restarted(mol_ios_audio_lifecycle_t* lifecycle) {
  if (lifecycle == NULL || !lifecycle->user_started) {
    return MOL_IOS_AUDIO_ACTION_NONE;
  }
  increment_route_revision(lifecycle);
  return MOL_IOS_AUDIO_ACTION_RESTORE_STATE;
}

uint32_t mol_ios_audio_lifecycle_media_services_reset(mol_ios_audio_lifecycle_t* lifecycle) {
  if (lifecycle == NULL || !lifecycle->user_started) {
    return MOL_IOS_AUDIO_ACTION_NONE;
  }
  increment_route_revision(lifecycle);
  if (lifecycle->ui_foreground || mol_ios_audio_lifecycle_allows_background(lifecycle)) {
    return MOL_IOS_AUDIO_ACTION_RESTART;
  }
  mol_ios_audio_lifecycle_stop(lifecycle);
  return MOL_IOS_AUDIO_ACTION_STOP;
}

uint32_t mol_ios_audio_lifecycle_restart_completed(mol_ios_audio_lifecycle_t* lifecycle,
                                                   bool succeeded) {
  if (lifecycle == NULL || !lifecycle->user_started) {
    return MOL_IOS_AUDIO_ACTION_NONE;
  }
  if (succeeded) {
    return MOL_IOS_AUDIO_ACTION_RESTORE_STATE;
  }
  mol_ios_audio_lifecycle_stop(lifecycle);
  return MOL_IOS_AUDIO_ACTION_STOP;
}

bool mol_ios_audio_lifecycle_allows_background(const mol_ios_audio_lifecycle_t* lifecycle) {
  return lifecycle != NULL && (lifecycle->playback_running ||
                               (lifecycle->metronome_enabled && lifecycle->transport_running));
}
