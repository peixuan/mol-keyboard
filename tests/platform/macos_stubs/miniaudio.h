// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_TESTS_MACOS_STUBS_MINIAUDIO_H_
#define MOL_TESTS_MACOS_STUBS_MINIAUDIO_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t ma_uint32;
typedef int32_t ma_result;
typedef uint8_t ma_bool8;

enum {
  MA_SUCCESS = 0,
  MA_ERROR = -1,
  MA_TRUE = 1,
  MA_FALSE = 0,
};

typedef enum ma_backend {
  ma_backend_wasapi,
  ma_backend_coreaudio,
  ma_backend_pulseaudio,
  ma_backend_alsa,
  ma_backend_jack,
  ma_backend_null,
} ma_backend;

typedef enum ma_device_type { ma_device_type_playback } ma_device_type;
typedef enum ma_format { ma_format_f32 } ma_format;
typedef enum ma_share_mode { ma_share_mode_shared } ma_share_mode;
typedef enum ma_performance_profile { ma_performance_profile_low_latency } ma_performance_profile;
typedef enum ma_thread_priority { ma_thread_priority_realtime } ma_thread_priority;
typedef enum ma_wasapi_usage { ma_wasapi_usage_pro_audio } ma_wasapi_usage;
typedef enum ma_device_notification_type {
  ma_device_notification_type_started,
  ma_device_notification_type_stopped,
  ma_device_notification_type_rerouted,
} ma_device_notification_type;

typedef union ma_device_id {
  uint8_t bytes[64];
} ma_device_id;

typedef struct ma_context_config {
  ma_thread_priority threadPriority;
} ma_context_config;

typedef struct ma_context {
  ma_backend backend;
} ma_context;

struct ma_device;
struct ma_device_notification;
typedef void (*ma_device_data_proc)(struct ma_device* device, void* output, const void* input,
                                    ma_uint32 frame_count);
typedef void (*ma_device_notification_proc)(const struct ma_device_notification* notification);

typedef struct ma_device_config {
  ma_uint32 sampleRate;
  ma_uint32 periodSizeInFrames;
  ma_uint32 periods;
  ma_performance_profile performanceProfile;
  ma_bool8 noPreSilencedOutputBuffer;
  ma_bool8 noFixedSizedCallback;
  ma_device_data_proc dataCallback;
  ma_device_notification_proc notificationCallback;
  void* pUserData;
  struct {
    const ma_device_id* pDeviceID;
    ma_format format;
    ma_uint32 channels;
    ma_share_mode shareMode;
  } playback;
  struct {
    ma_wasapi_usage usage;
  } wasapi;
} ma_device_config;

typedef struct ma_device {
  ma_uint32 sampleRate;
  void* pUserData;
  ma_device_data_proc dataCallback;
  ma_device_notification_proc notificationCallback;
  struct {
    char name[256];
    ma_uint32 internalPeriodSizeInFrames;
    ma_uint32 internalPeriods;
  } playback;
} ma_device;

typedef struct ma_device_notification {
  ma_device* pDevice;
  ma_device_notification_type type;
} ma_device_notification;

typedef struct ma_device_info {
  ma_device_id id;
  char name[256];
  ma_bool8 isDefault;
} ma_device_info;

ma_context_config ma_context_config_init(void);
ma_result ma_context_init(const ma_backend* backends, ma_uint32 backend_count,
                          const ma_context_config* config, ma_context* context);
void ma_context_uninit(ma_context* context);
ma_device_config ma_device_config_init(ma_device_type type);
ma_result ma_device_init(ma_context* context, const ma_device_config* config, ma_device* device);
void ma_device_uninit(ma_device* device);
ma_result ma_device_start(ma_device* device);
ma_result ma_device_stop(ma_device* device);
ma_result ma_context_get_devices(ma_context* context, ma_device_info** playback_devices,
                                 ma_uint32* playback_count, ma_device_info** capture_devices,
                                 ma_uint32* capture_count);
const char* ma_get_backend_name(ma_backend backend);
const char* ma_result_description(ma_result result);

#ifdef __cplusplus
}
#endif

#endif  // MOL_TESTS_MACOS_STUBS_MINIAUDIO_H_
