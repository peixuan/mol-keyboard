/* SPDX-License-Identifier: Apache-2.0 */
#include "a2dp_source.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <string.h>

#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define MOL_A2DP_ADDRESS_SIZE 6u
#define MOL_A2DP_RECONNECT_MS 5000u
#define MOL_A2DP_CONNECT_TIMEOUT_MS 15000u
#define MOL_A2DP_CONTROL_PERIOD_MS 250u
#define MOL_A2DP_CONTROL_TIMEOUT_MS 5000u
#define MOL_A2DP_MAX_SBC_BITPOOL 53u
#define MOL_A2DP_AVRC_TRANSACTION_LABEL 0u

typedef struct mol_a2dp_atomic_stats {
  atomic_uint_least32_t discovered_sinks;
  atomic_uint_least32_t connection_attempts;
  atomic_uint_least32_t connections;
  atomic_uint_least32_t disconnects;
  atomic_uint_least32_t connection_failures;
  atomic_uint_least32_t codec_rejections;
  atomic_uint_least32_t media_start_requests;
  atomic_uint_least32_t media_control_failures;
  atomic_uint_least32_t pcm_submitted_bytes;
  atomic_uint_least32_t pcm_dropped_bytes;
  atomic_uint_least32_t pcm_callbacks;
  atomic_uint_least32_t pcm_underruns;
  atomic_uint_least32_t pcm_silence_bytes;
  atomic_uint_least32_t pcm_buffer_high_water;
  atomic_uint_least32_t avrc_connections;
  atomic_uint_least32_t avrc_capability_responses;
  atomic_uint_least32_t avrc_events;
  atomic_uint_least32_t authentication_failures;
  atomic_uint_least32_t sink_delay_tenths_ms;
} mol_a2dp_atomic_stats_t;

static const char* const kTag = "mol-a2dp";
static uint8_t pcm_storage[CONFIG_MOL_A2DP_PCM_BUFFER_BYTES + 1u];
static StaticStreamBuffer_t pcm_stream_control;
static StreamBufferHandle_t pcm_stream;
static StackType_t control_task_stack[CONFIG_MOL_A2DP_CONTROL_TASK_STACK_SIZE];
static StaticTask_t control_task_control;
static TaskHandle_t control_task_handle;
static atomic_bool output_enabled;
static atomic_bool profile_ready;
static atomic_bool preferred_peer_valid;
static atomic_bool candidate_valid;
static atomic_bool discovery_active;
static atomic_bool connecting;
static atomic_bool connected;
static atomic_bool disconnecting;
static atomic_bool codec_ready;
static atomic_bool codec_failed;
static atomic_bool codec_configuration_pending;
static atomic_bool source_ready;
static atomic_bool start_requested;
static atomic_bool streaming;
static atomic_bool new_peer_pending;
static atomic_bool avrc_capability_request_pending;
static atomic_uint_least32_t control_pending;
static atomic_uint_least32_t control_pending_tick;
static atomic_uint_least32_t next_connect_tick;
static atomic_uint_least32_t connect_started_tick;
static atomic_uint_least32_t discard_bytes;
static uint8_t preferred_peer[MOL_A2DP_ADDRESS_SIZE];
static uint8_t candidate_peer[MOL_A2DP_ADDRESS_SIZE];
static uint8_t active_peer[MOL_A2DP_ADDRESS_SIZE];
static uint8_t new_peer[MOL_A2DP_ADDRESS_SIZE];
static esp_a2d_mcc_t pending_codec_configuration;
static esp_a2d_conn_hdl_t pending_codec_connection;
static mol_a2dp_atomic_stats_t stats;
static bool started;

static void update_max(atomic_uint_least32_t* maximum, uint32_t value) {
  uint_least32_t observed = atomic_load_explicit(maximum, memory_order_relaxed);
  while ((uint_least32_t)value > observed &&
         !atomic_compare_exchange_weak_explicit(maximum, &observed, value, memory_order_relaxed,
                                                memory_order_relaxed)) {
  }
}

static bool tick_reached(TickType_t now, uint_least32_t deadline) {
  return (int32_t)((uint32_t)now - (uint32_t)deadline) >= 0;
}

static uint8_t configured_sample_frequency(void) {
  switch (CONFIG_MOL_I2S_SAMPLE_RATE) {
    case 16000:
      return ESP_A2D_SBC_CIE_SF_16K;
    case 32000:
      return ESP_A2D_SBC_CIE_SF_32K;
    case 44100:
      return ESP_A2D_SBC_CIE_SF_44K;
    case 48000:
      return ESP_A2D_SBC_CIE_SF_48K;
    default:
      return 0u;
  }
}

static uint8_t first_supported(uint8_t capabilities, const uint8_t* preference,
                               size_t preference_count) {
  size_t index;
  for (index = 0u; index < preference_count; ++index) {
    if ((capabilities & preference[index]) != 0u) {
      return preference[index];
    }
  }
  return 0u;
}

static bool sink_supports_configured_codec(const esp_a2d_mcc_t* sink, esp_a2d_mcc_t* preferred) {
  static const uint8_t channel_preferences[] = {ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO,
                                                ESP_A2D_SBC_CIE_CH_MODE_STEREO,
                                                ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL};
  static const uint8_t block_preferences[] = {
      ESP_A2D_SBC_CIE_BLOCK_LEN_16, ESP_A2D_SBC_CIE_BLOCK_LEN_12, ESP_A2D_SBC_CIE_BLOCK_LEN_8,
      ESP_A2D_SBC_CIE_BLOCK_LEN_4};
  static const uint8_t subband_preferences[] = {ESP_A2D_SBC_CIE_NUM_SUBBANDS_8,
                                                ESP_A2D_SBC_CIE_NUM_SUBBANDS_4};
  static const uint8_t allocation_preferences[] = {ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS,
                                                   ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR};
  const esp_a2d_cie_sbc_t* capabilities;
  esp_a2d_cie_sbc_t* configuration;
  uint8_t sample_frequency = configured_sample_frequency();

  if (sink == NULL || preferred == NULL || sink->type != ESP_A2D_MCT_SBC ||
      sample_frequency == 0u) {
    return false;
  }
  capabilities = &sink->cie.sbc_info;
  if ((capabilities->samp_freq & sample_frequency) == 0u) {
    return false;
  }

  memset(preferred, 0, sizeof(*preferred));
  preferred->type = ESP_A2D_MCT_SBC;
  configuration = &preferred->cie.sbc_info;
  configuration->samp_freq = sample_frequency;
  configuration->ch_mode =
      first_supported(capabilities->ch_mode, channel_preferences, sizeof(channel_preferences));
  configuration->block_len =
      first_supported(capabilities->block_len, block_preferences, sizeof(block_preferences));
  configuration->num_subbands =
      first_supported(capabilities->num_subbands, subband_preferences, sizeof(subband_preferences));
  configuration->alloc_mthd = first_supported(capabilities->alloc_mthd, allocation_preferences,
                                              sizeof(allocation_preferences));
  configuration->min_bitpool = capabilities->min_bitpool > 2u ? capabilities->min_bitpool : 2u;
  configuration->max_bitpool = capabilities->max_bitpool < MOL_A2DP_MAX_SBC_BITPOOL
                                   ? capabilities->max_bitpool
                                   : MOL_A2DP_MAX_SBC_BITPOOL;
  return configuration->ch_mode != 0u && configuration->block_len != 0u &&
         configuration->num_subbands != 0u && configuration->alloc_mthd != 0u &&
         configuration->min_bitpool <= configuration->max_bitpool;
}

static bool discovery_result_is_sink(const esp_bt_gap_cb_param_t* parameter) {
  int property_index;
  for (property_index = 0; property_index < parameter->disc_res.num_prop; ++property_index) {
    const esp_bt_gap_dev_prop_t* property = &parameter->disc_res.prop[property_index];
    if (property->type == ESP_BT_GAP_DEV_PROP_COD && property->len == sizeof(uint32_t)) {
      uint32_t cod;
      memcpy(&cod, property->val, sizeof(cod));
      return esp_bt_gap_is_valid_cod(cod) &&
             (esp_bt_gap_get_cod_srvc(cod) & ESP_BT_COD_SRVC_RENDERING) != 0u;
    }
  }
  return false;
}

static bool address_matches_flagged_peer(const uint8_t address[MOL_A2DP_ADDRESS_SIZE],
                                         const uint8_t peer[MOL_A2DP_ADDRESS_SIZE],
                                         const atomic_bool* valid) {
  return atomic_load_explicit(valid, memory_order_acquire) &&
         memcmp(address, peer, MOL_A2DP_ADDRESS_SIZE) == 0;
}

bool mol_a2dp_source_is_peer(const uint8_t address[6]) {
  if (address == NULL) {
    return false;
  }
  return address_matches_flagged_peer(address, preferred_peer, &preferred_peer_valid) ||
         address_matches_flagged_peer(address, candidate_peer, &candidate_valid) ||
         address_matches_flagged_peer(address, active_peer, &connected);
}

static int32_t pcm_data_callback(uint8_t* output, int32_t length) {
  size_t received;
  size_t requested;
  uint8_t flush_scratch[64];
  if (length < 0) {
    while (xStreamBufferReceive(pcm_stream, flush_scratch, sizeof(flush_scratch), 0u) != 0u) {
    }
    atomic_store_explicit(&discard_bytes, 0u, memory_order_release);
    return 0;
  }
  if (output == NULL || length == 0) {
    return 0;
  }
  requested = (size_t)length;
  atomic_fetch_add_explicit(&stats.pcm_callbacks, 1u, memory_order_relaxed);
  if (!atomic_load_explicit(&output_enabled, memory_order_acquire) ||
      !atomic_load_explicit(&streaming, memory_order_acquire)) {
    memset(output, 0, requested);
    atomic_fetch_add_explicit(&stats.pcm_silence_bytes, (uint_least32_t)requested,
                              memory_order_relaxed);
    return length;
  }

  received = 0u;
  while (received < requested) {
    uint_least32_t pending_discard = atomic_load_explicit(&discard_bytes, memory_order_acquire);
    size_t discarded;
    size_t amount;
    if (pending_discard == 0u) {
      break;
    }
    amount = requested - received;
    if (amount > pending_discard) {
      amount = (size_t)pending_discard;
    }
    discarded = xStreamBufferReceive(pcm_stream, output + received, amount, 0u);
    if (discarded == 0u) {
      break;
    }
    memset(output + received, 0, discarded);
    received += discarded;
    {
      uint_least32_t observed = pending_discard;
      uint_least32_t remaining;
      do {
        remaining = observed > discarded ? observed - (uint_least32_t)discarded : 0u;
      } while (!atomic_compare_exchange_weak_explicit(&discard_bytes, &observed, remaining,
                                                      memory_order_release, memory_order_acquire));
    }
    atomic_fetch_add_explicit(&stats.pcm_silence_bytes, (uint_least32_t)discarded,
                              memory_order_relaxed);
  }
  received += xStreamBufferReceive(pcm_stream, output + received, requested - received, 0u);
  if (received < requested) {
    const size_t missing = requested - received;
    memset(output + received, 0, missing);
    atomic_fetch_add_explicit(&stats.pcm_underruns, 1u, memory_order_relaxed);
    atomic_fetch_add_explicit(&stats.pcm_silence_bytes, (uint_least32_t)missing,
                              memory_order_relaxed);
  }
  return length;
}

static void a2dp_event_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* parameter) {
  switch (event) {
    case ESP_A2D_PROF_STATE_EVT:
      atomic_store_explicit(&profile_ready,
                            parameter->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS,
                            memory_order_release);
      ESP_LOGI(kTag, "profile %s",
               parameter->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS ? "ready" : "stopped");
      break;
    case ESP_A2D_CONNECTION_STATE_EVT:
      if (parameter->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        const bool changed =
            !atomic_load_explicit(&preferred_peer_valid, memory_order_acquire) ||
            memcmp(preferred_peer, parameter->conn_stat.remote_bda, MOL_A2DP_ADDRESS_SIZE) != 0;
        memcpy(active_peer, parameter->conn_stat.remote_bda, MOL_A2DP_ADDRESS_SIZE);
        atomic_store_explicit(&candidate_valid, false, memory_order_release);
        atomic_store_explicit(&connecting, false, memory_order_release);
        atomic_store_explicit(&disconnecting, false, memory_order_release);
        atomic_store_explicit(&codec_ready, false, memory_order_release);
        atomic_store_explicit(&codec_failed, false, memory_order_release);
        atomic_store_explicit(&codec_configuration_pending, false, memory_order_release);
        atomic_store_explicit(&source_ready, false, memory_order_release);
        atomic_store_explicit(&start_requested, false, memory_order_release);
        atomic_store_explicit(&streaming, false, memory_order_release);
        atomic_store_explicit(&control_pending, ESP_A2D_MEDIA_CTRL_NONE, memory_order_release);
        atomic_store_explicit(&connected, true, memory_order_release);
        atomic_fetch_add_explicit(&stats.connections, 1u, memory_order_relaxed);
        if (changed) {
          memcpy(new_peer, parameter->conn_stat.remote_bda, MOL_A2DP_ADDRESS_SIZE);
          atomic_store_explicit(&new_peer_pending, true, memory_order_release);
          atomic_store_explicit(&preferred_peer_valid, false, memory_order_release);
          memcpy(preferred_peer, parameter->conn_stat.remote_bda, MOL_A2DP_ADDRESS_SIZE);
          atomic_store_explicit(&preferred_peer_valid, true, memory_order_release);
        }
        ESP_LOGI(kTag, "connected to %02x:%02x:%02x:%02x:%02x:%02x mtu=%u", active_peer[0],
                 active_peer[1], active_peer[2], active_peer[3], active_peer[4], active_peer[5],
                 (unsigned int)parameter->conn_stat.audio_mtu);
      } else if (parameter->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
        const bool was_connected =
            atomic_exchange_explicit(&connected, false, memory_order_acq_rel);
        const bool was_connecting =
            atomic_exchange_explicit(&connecting, false, memory_order_acq_rel);
        atomic_store_explicit(&disconnecting, false, memory_order_release);
        atomic_store_explicit(&codec_ready, false, memory_order_release);
        atomic_store_explicit(&codec_failed, false, memory_order_release);
        atomic_store_explicit(&codec_configuration_pending, false, memory_order_release);
        atomic_store_explicit(&source_ready, false, memory_order_release);
        atomic_store_explicit(&start_requested, false, memory_order_release);
        atomic_store_explicit(&streaming, false, memory_order_release);
        atomic_store_explicit(&control_pending, ESP_A2D_MEDIA_CTRL_NONE, memory_order_release);
        if (!atomic_load_explicit(&preferred_peer_valid, memory_order_acquire)) {
          atomic_store_explicit(&candidate_valid, false, memory_order_release);
        }
        atomic_store_explicit(
            &next_connect_tick,
            (uint_least32_t)(xTaskGetTickCount() + pdMS_TO_TICKS(MOL_A2DP_RECONNECT_MS)),
            memory_order_release);
        if (was_connected) {
          atomic_fetch_add_explicit(&stats.disconnects, 1u, memory_order_relaxed);
        } else if (was_connecting) {
          atomic_fetch_add_explicit(&stats.connection_failures, 1u, memory_order_relaxed);
        }
        ESP_LOGW(kTag, "disconnected: reason=%d", (int)parameter->conn_stat.disc_rsn);
      }
      break;
    case ESP_A2D_AUDIO_STATE_EVT:
      if (parameter->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
        atomic_store_explicit(&discard_bytes,
                              (uint_least32_t)xStreamBufferBytesAvailable(pcm_stream),
                              memory_order_release);
        atomic_store_explicit(&streaming, true, memory_order_release);
      } else {
        atomic_store_explicit(&streaming, false, memory_order_release);
        atomic_store_explicit(&source_ready, false, memory_order_release);
        atomic_store_explicit(&start_requested, false, memory_order_release);
      }
      break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT: {
      uint_least32_t expected = (uint_least32_t)parameter->media_ctrl_stat.cmd;
      if (!atomic_compare_exchange_strong_explicit(&control_pending, &expected,
                                                   ESP_A2D_MEDIA_CTRL_NONE, memory_order_acq_rel,
                                                   memory_order_acquire)) {
        atomic_fetch_add_explicit(&stats.media_control_failures, 1u, memory_order_relaxed);
        break;
      }
      if (parameter->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) {
        const bool ready = parameter->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS;
        atomic_store_explicit(&source_ready, ready, memory_order_release);
        if (!ready) {
          atomic_fetch_add_explicit(&stats.media_control_failures, 1u, memory_order_relaxed);
        }
      } else if (parameter->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START &&
                 parameter->media_ctrl_stat.status != ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
        atomic_store_explicit(&source_ready, false, memory_order_release);
        atomic_store_explicit(&start_requested, false, memory_order_release);
        atomic_fetch_add_explicit(&stats.media_control_failures, 1u, memory_order_relaxed);
      }
      break;
    }
    case ESP_A2D_REPORT_SNK_CODEC_CAPS_EVT: {
      esp_a2d_mcc_t preferred;
      if (!sink_supports_configured_codec(&parameter->a2d_report_snk_codec_caps_stat.mcc,
                                          &preferred)) {
        atomic_store_explicit(&codec_ready, false, memory_order_release);
        atomic_store_explicit(&codec_failed, true, memory_order_release);
        atomic_fetch_add_explicit(&stats.codec_rejections, 1u, memory_order_relaxed);
        ESP_LOGE(kTag, "sink cannot accept %d Hz SBC stereo; retaining I2S fallback",
                 CONFIG_MOL_I2S_SAMPLE_RATE);
      } else {
        pending_codec_configuration = preferred;
        pending_codec_connection = parameter->a2d_report_snk_codec_caps_stat.conn_hdl;
        atomic_store_explicit(&codec_configuration_pending, true, memory_order_release);
      }
      break;
    }
    case ESP_A2D_SRC_SET_PREF_MCC_EVT:
      if (parameter->a2d_set_pref_mcc_stat.set_status == ESP_BT_STATUS_SUCCESS &&
          atomic_load_explicit(&connected, memory_order_acquire)) {
        atomic_store_explicit(&codec_ready, true, memory_order_release);
      } else {
        atomic_store_explicit(&codec_ready, false, memory_order_release);
        atomic_store_explicit(&codec_failed, true, memory_order_release);
        atomic_fetch_add_explicit(&stats.codec_rejections, 1u, memory_order_relaxed);
      }
      break;
    case ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT:
      atomic_store_explicit(&stats.sink_delay_tenths_ms,
                            parameter->a2d_report_delay_value_stat.delay_value,
                            memory_order_relaxed);
      break;
    default:
      break;
  }
}

static void avrc_event_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* parameter) {
  atomic_fetch_add_explicit(&stats.avrc_events, 1u, memory_order_relaxed);
  if (event == ESP_AVRC_CT_CONNECTION_STATE_EVT && parameter->conn_stat.connected) {
    atomic_fetch_add_explicit(&stats.avrc_connections, 1u, memory_order_relaxed);
    atomic_store_explicit(&avrc_capability_request_pending, true, memory_order_release);
  } else if (event == ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT) {
    atomic_fetch_add_explicit(&stats.avrc_capability_responses, 1u, memory_order_relaxed);
  } else if (event == ESP_AVRC_CT_PROF_STATE_EVT) {
    ESP_LOGI(kTag, "AVRCP controller %s",
             parameter->avrc_ct_init_stat.state == ESP_AVRC_INIT_SUCCESS ? "ready" : "stopped");
  }
}

static bool send_media_control(esp_a2d_media_ctrl_t command) {
  uint_least32_t expected = ESP_A2D_MEDIA_CTRL_NONE;
  if (!atomic_compare_exchange_strong_explicit(&control_pending, &expected, command,
                                               memory_order_acq_rel, memory_order_acquire)) {
    return false;
  }
  atomic_store_explicit(&control_pending_tick, (uint_least32_t)xTaskGetTickCount(),
                        memory_order_release);
  if (esp_a2d_media_ctrl(command) != ESP_OK) {
    atomic_store_explicit(&control_pending, ESP_A2D_MEDIA_CTRL_NONE, memory_order_release);
    atomic_fetch_add_explicit(&stats.media_control_failures, 1u, memory_order_relaxed);
    return false;
  }
  return true;
}

static void control_task(void* context) {
  (void)context;
  for (;;) {
    const TickType_t now = xTaskGetTickCount();
    const uint_least32_t pending = atomic_load_explicit(&control_pending, memory_order_acquire);
    if (pending != ESP_A2D_MEDIA_CTRL_NONE &&
        (uint32_t)now -
                (uint32_t)atomic_load_explicit(&control_pending_tick, memory_order_acquire) >=
            (uint32_t)pdMS_TO_TICKS(MOL_A2DP_CONTROL_TIMEOUT_MS)) {
      atomic_store_explicit(&control_pending, ESP_A2D_MEDIA_CTRL_NONE, memory_order_release);
      atomic_store_explicit(&source_ready, false, memory_order_release);
      atomic_store_explicit(&start_requested, false, memory_order_release);
      atomic_fetch_add_explicit(&stats.media_control_failures, 1u, memory_order_relaxed);
    }

    if (atomic_exchange_explicit(&avrc_capability_request_pending, false, memory_order_acq_rel) &&
        esp_avrc_ct_send_get_rn_capabilities_cmd(MOL_A2DP_AVRC_TRANSACTION_LABEL) != ESP_OK) {
      atomic_fetch_add_explicit(&stats.media_control_failures, 1u, memory_order_relaxed);
    }
    if (atomic_exchange_explicit(&codec_configuration_pending, false, memory_order_acq_rel) &&
        atomic_load_explicit(&connected, memory_order_acquire) &&
        esp_a2d_source_set_pref_mcc(pending_codec_connection, &pending_codec_configuration) !=
            ESP_OK) {
      atomic_store_explicit(&codec_failed, true, memory_order_release);
      atomic_fetch_add_explicit(&stats.codec_rejections, 1u, memory_order_relaxed);
    }
    if (atomic_load_explicit(&connecting, memory_order_acquire) &&
        !atomic_load_explicit(&disconnecting, memory_order_acquire) &&
        (uint32_t)now -
                (uint32_t)atomic_load_explicit(&connect_started_tick, memory_order_acquire) >=
            (uint32_t)pdMS_TO_TICKS(MOL_A2DP_CONNECT_TIMEOUT_MS)) {
      uint8_t address[MOL_A2DP_ADDRESS_SIZE];
      memcpy(address, candidate_peer, sizeof(address));
      atomic_store_explicit(&disconnecting, true, memory_order_release);
      if (esp_a2d_source_disconnect(address) != ESP_OK) {
        atomic_store_explicit(&disconnecting, false, memory_order_release);
        atomic_store_explicit(&connecting, false, memory_order_release);
        atomic_store_explicit(&next_connect_tick,
                              (uint_least32_t)(now + pdMS_TO_TICKS(MOL_A2DP_RECONNECT_MS)),
                              memory_order_release);
        atomic_fetch_add_explicit(&stats.connection_failures, 1u, memory_order_relaxed);
      }
    }

    if (!atomic_load_explicit(&output_enabled, memory_order_acquire)) {
      if (atomic_load_explicit(&connected, memory_order_acquire) &&
          !atomic_exchange_explicit(&disconnecting, true, memory_order_acq_rel)) {
        uint8_t address[MOL_A2DP_ADDRESS_SIZE];
        memcpy(address, active_peer, sizeof(address));
        if (esp_a2d_source_disconnect(address) != ESP_OK) {
          atomic_store_explicit(&disconnecting, false, memory_order_release);
          atomic_fetch_add_explicit(&stats.connection_failures, 1u, memory_order_relaxed);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(MOL_A2DP_CONTROL_PERIOD_MS));
      continue;
    }

    if (atomic_load_explicit(&profile_ready, memory_order_acquire) &&
        !atomic_load_explicit(&connected, memory_order_acquire) &&
        !atomic_load_explicit(&connecting, memory_order_acquire)) {
      if (!atomic_load_explicit(&candidate_valid, memory_order_acquire) &&
          atomic_load_explicit(&preferred_peer_valid, memory_order_acquire)) {
        memcpy(candidate_peer, preferred_peer, sizeof(candidate_peer));
        atomic_store_explicit(&candidate_valid, true, memory_order_release);
      }
      if (atomic_load_explicit(&candidate_valid, memory_order_acquire) &&
          !atomic_load_explicit(&discovery_active, memory_order_acquire) &&
          tick_reached(now, atomic_load_explicit(&next_connect_tick, memory_order_acquire))) {
        uint8_t address[MOL_A2DP_ADDRESS_SIZE];
        memcpy(address, candidate_peer, sizeof(address));
        atomic_store_explicit(&disconnecting, false, memory_order_release);
        atomic_store_explicit(&connecting, true, memory_order_release);
        atomic_store_explicit(&connect_started_tick, (uint_least32_t)now, memory_order_release);
        atomic_fetch_add_explicit(&stats.connection_attempts, 1u, memory_order_relaxed);
        if (esp_a2d_source_connect(address) != ESP_OK) {
          atomic_store_explicit(&connecting, false, memory_order_release);
          atomic_store_explicit(&next_connect_tick,
                                (uint_least32_t)(now + pdMS_TO_TICKS(MOL_A2DP_RECONNECT_MS)),
                                memory_order_release);
          atomic_fetch_add_explicit(&stats.connection_failures, 1u, memory_order_relaxed);
        }
      }
    }

    if (atomic_load_explicit(&connected, memory_order_acquire) &&
        atomic_load_explicit(&codec_failed, memory_order_acquire) &&
        !atomic_exchange_explicit(&disconnecting, true, memory_order_acq_rel)) {
      uint8_t address[MOL_A2DP_ADDRESS_SIZE];
      memcpy(address, active_peer, sizeof(address));
      if (esp_a2d_source_disconnect(address) != ESP_OK) {
        atomic_store_explicit(&disconnecting, false, memory_order_release);
        atomic_fetch_add_explicit(&stats.connection_failures, 1u, memory_order_relaxed);
      }
    } else if (atomic_load_explicit(&connected, memory_order_acquire) &&
               atomic_load_explicit(&codec_ready, memory_order_acquire) &&
               !atomic_load_explicit(&streaming, memory_order_acquire) &&
               atomic_load_explicit(&control_pending, memory_order_acquire) ==
                   ESP_A2D_MEDIA_CTRL_NONE) {
      if (!atomic_load_explicit(&source_ready, memory_order_acquire)) {
        (void)send_media_control(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
      } else if (!atomic_load_explicit(&start_requested, memory_order_acquire)) {
        atomic_store_explicit(&start_requested, true, memory_order_release);
        if (send_media_control(ESP_A2D_MEDIA_CTRL_START)) {
          atomic_fetch_add_explicit(&stats.media_start_requests, 1u, memory_order_relaxed);
        } else {
          atomic_store_explicit(&start_requested, false, memory_order_release);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(MOL_A2DP_CONTROL_PERIOD_MS));
  }
}

esp_err_t mol_a2dp_source_start(const uint8_t preferred_address[6], bool preferred_valid,
                                bool enabled) {
  esp_err_t result;
  if (started || (preferred_valid && preferred_address == NULL)) {
    return ESP_ERR_INVALID_ARG;
  }
  if (configured_sample_frequency() == 0u) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  if ((CONFIG_MOL_A2DP_PCM_BUFFER_BYTES % 4) != 0) {
    return ESP_ERR_INVALID_SIZE;
  }
  started = true;
  if (preferred_valid) {
    memcpy(preferred_peer, preferred_address, sizeof(preferred_peer));
  } else {
    memset(preferred_peer, 0, sizeof(preferred_peer));
  }
  atomic_store_explicit(&preferred_peer_valid, preferred_valid, memory_order_release);
  atomic_store_explicit(&output_enabled, enabled, memory_order_release);
  atomic_store_explicit(&next_connect_tick, (uint_least32_t)xTaskGetTickCount(),
                        memory_order_release);
  pcm_stream = xStreamBufferCreateStatic(sizeof(pcm_storage), 1u, pcm_storage, &pcm_stream_control);
  if (pcm_stream == NULL) {
    return ESP_ERR_NO_MEM;
  }

  result = esp_avrc_ct_register_callback(avrc_event_callback);
  if (result == ESP_OK) {
    result = esp_avrc_ct_init();
  }
  if (result == ESP_OK) {
    result = esp_a2d_register_callback(a2dp_event_callback);
  }
  if (result == ESP_OK) {
    result = esp_a2d_source_init();
  }
  if (result == ESP_OK) {
    result = esp_a2d_source_register_data_callback(pcm_data_callback);
  }
  if (result != ESP_OK) {
    return result;
  }

  control_task_handle = xTaskCreateStaticPinnedToCore(
      control_task, "mol-a2dp", CONFIG_MOL_A2DP_CONTROL_TASK_STACK_SIZE, NULL,
      CONFIG_MOL_A2DP_CONTROL_TASK_PRIORITY, control_task_stack, &control_task_control,
      CONFIG_MOL_A2DP_CONTROL_TASK_CORE);
  return control_task_handle != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

void mol_a2dp_source_set_enabled(bool enabled) {
  atomic_store_explicit(&output_enabled, enabled, memory_order_release);
}

void mol_a2dp_source_submit_pcm(const int16_t* samples, size_t byte_count) {
  size_t sent;
  uint32_t available;
  if (samples == NULL || byte_count == 0u || pcm_stream == NULL ||
      !atomic_load_explicit(&output_enabled, memory_order_acquire) ||
      !atomic_load_explicit(&streaming, memory_order_acquire)) {
    return;
  }
  sent = xStreamBufferSend(pcm_stream, samples, byte_count, 0u);
  atomic_fetch_add_explicit(&stats.pcm_submitted_bytes, (uint_least32_t)sent, memory_order_relaxed);
  if (sent < byte_count) {
    atomic_fetch_add_explicit(&stats.pcm_dropped_bytes, (uint_least32_t)(byte_count - sent),
                              memory_order_relaxed);
  }
  available = (uint32_t)xStreamBufferBytesAvailable(pcm_stream);
  update_max(&stats.pcm_buffer_high_water, available);
}

bool mol_a2dp_source_needs_discovery(void) {
  const TickType_t now = xTaskGetTickCount();
  return atomic_load_explicit(&output_enabled, memory_order_acquire) &&
         atomic_load_explicit(&profile_ready, memory_order_acquire) &&
         !atomic_load_explicit(&preferred_peer_valid, memory_order_acquire) &&
         !atomic_load_explicit(&candidate_valid, memory_order_acquire) &&
         !atomic_load_explicit(&connecting, memory_order_acquire) &&
         !atomic_load_explicit(&connected, memory_order_acquire) &&
         tick_reached(now, atomic_load_explicit(&next_connect_tick, memory_order_acquire));
}

bool mol_a2dp_source_handle_gap_event(esp_bt_gap_cb_event_t event,
                                      esp_bt_gap_cb_param_t* parameter) {
  if (parameter == NULL) {
    return false;
  }
  if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT) {
    atomic_store_explicit(&discovery_active,
                          parameter->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED,
                          memory_order_release);
  } else if (event == ESP_BT_GAP_DISC_RES_EVT && mol_a2dp_source_needs_discovery() &&
             discovery_result_is_sink(parameter)) {
    memcpy(candidate_peer, parameter->disc_res.bda, sizeof(candidate_peer));
    atomic_store_explicit(&candidate_valid, true, memory_order_release);
    atomic_fetch_add_explicit(&stats.discovered_sinks, 1u, memory_order_relaxed);
    (void)esp_bt_gap_cancel_discovery();
  } else if (event == ESP_BT_GAP_PIN_REQ_EVT && mol_a2dp_source_is_peer(parameter->pin_req.bda)) {
    esp_bt_pin_code_t pin = {0u};
    uint8_t length = 16u;
    if (!parameter->pin_req.min_16_digit) {
      memset(pin, '0', 4u);
      length = 4u;
    }
    (void)esp_bt_gap_pin_reply(parameter->pin_req.bda, true, length, pin);
    return true;
  } else if (event == ESP_BT_GAP_CFM_REQ_EVT && mol_a2dp_source_is_peer(parameter->cfm_req.bda)) {
    (void)esp_bt_gap_ssp_confirm_reply(parameter->cfm_req.bda, true);
    return true;
  } else if (event == ESP_BT_GAP_AUTH_CMPL_EVT &&
             mol_a2dp_source_is_peer(parameter->auth_cmpl.bda)) {
    if (parameter->auth_cmpl.stat != ESP_BT_STATUS_SUCCESS) {
      atomic_fetch_add_explicit(&stats.authentication_failures, 1u, memory_order_relaxed);
      ESP_LOGW(kTag, "authentication failed: status=%d", (int)parameter->auth_cmpl.stat);
    }
    return true;
  }
  return false;
}

bool mol_a2dp_source_take_new_peer(uint8_t address[6]) {
  if (address == NULL ||
      !atomic_exchange_explicit(&new_peer_pending, false, memory_order_acq_rel)) {
    return false;
  }
  memcpy(address, new_peer, MOL_A2DP_ADDRESS_SIZE);
  return true;
}

mol_a2dp_source_stats_t mol_a2dp_source_stats(void) {
  mol_a2dp_source_stats_t snapshot;
#define MOL_A2DP_STAT(field) \
  snapshot.field = (uint32_t)atomic_load_explicit(&stats.field, memory_order_relaxed)
  MOL_A2DP_STAT(discovered_sinks);
  MOL_A2DP_STAT(connection_attempts);
  MOL_A2DP_STAT(connections);
  MOL_A2DP_STAT(disconnects);
  MOL_A2DP_STAT(connection_failures);
  MOL_A2DP_STAT(codec_rejections);
  MOL_A2DP_STAT(media_start_requests);
  MOL_A2DP_STAT(media_control_failures);
  MOL_A2DP_STAT(pcm_submitted_bytes);
  MOL_A2DP_STAT(pcm_dropped_bytes);
  MOL_A2DP_STAT(pcm_callbacks);
  MOL_A2DP_STAT(pcm_underruns);
  MOL_A2DP_STAT(pcm_silence_bytes);
  MOL_A2DP_STAT(pcm_buffer_high_water);
  MOL_A2DP_STAT(avrc_connections);
  MOL_A2DP_STAT(avrc_capability_responses);
  MOL_A2DP_STAT(avrc_events);
  MOL_A2DP_STAT(authentication_failures);
  MOL_A2DP_STAT(sink_delay_tenths_ms);
#undef MOL_A2DP_STAT
  snapshot.pcm_buffer_bytes =
      pcm_stream != NULL ? (uint32_t)xStreamBufferBytesAvailable(pcm_stream) : 0u;
  snapshot.control_stack_high_water =
      control_task_handle != NULL ? (uint32_t)uxTaskGetStackHighWaterMark(control_task_handle) : 0u;
  return snapshot;
}
