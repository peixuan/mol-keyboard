/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_A2DP_SOURCE_H_
#define MOL_ESP32_A2DP_SOURCE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_gap_bt_api.h"

typedef struct mol_a2dp_source_stats {
  uint32_t discovered_sinks;
  uint32_t connection_attempts;
  uint32_t connections;
  uint32_t disconnects;
  uint32_t connection_failures;
  uint32_t codec_rejections;
  uint32_t media_start_requests;
  uint32_t media_control_failures;
  uint32_t pcm_submitted_bytes;
  uint32_t pcm_dropped_bytes;
  uint32_t pcm_callbacks;
  uint32_t pcm_underruns;
  uint32_t pcm_silence_bytes;
  uint32_t pcm_buffer_high_water;
  uint32_t pcm_buffer_bytes;
  uint32_t avrc_connections;
  uint32_t avrc_capability_responses;
  uint32_t avrc_events;
  uint32_t authentication_failures;
  uint32_t sink_delay_tenths_ms;
  uint32_t control_stack_high_water;
} mol_a2dp_source_stats_t;

/** Initializes the original ESP32 A2DP Source profile after Bluedroid is ready. */
esp_err_t mol_a2dp_source_start(const uint8_t preferred_address[6], bool preferred_valid,
                                bool output_enabled);

/** Enables or disables Bluetooth audio routing without changing the I2S fallback. */
void mol_a2dp_source_set_enabled(bool enabled);

/** Performs a bounded, non-blocking PCM16 stereo write from the audio task. */
void mol_a2dp_source_submit_pcm(const int16_t* samples, size_t byte_count);

/** Returns true when the shared Classic GAP scanner should search for a sink. */
bool mol_a2dp_source_needs_discovery(void);

/** Observes Classic GAP events and returns true when a pairing event was consumed. */
bool mol_a2dp_source_handle_gap_event(esp_bt_gap_cb_event_t event,
                                      esp_bt_gap_cb_param_t* parameter);

/** Tests whether an address belongs to the configured, candidate, or active sink. */
bool mol_a2dp_source_is_peer(const uint8_t address[6]);

/** Retrieves a newly connected sink address once for persistent storage. */
bool mol_a2dp_source_take_new_peer(uint8_t address[6]);

mol_a2dp_source_stats_t mol_a2dp_source_stats(void);

#endif /* MOL_ESP32_A2DP_SOURCE_H_ */
