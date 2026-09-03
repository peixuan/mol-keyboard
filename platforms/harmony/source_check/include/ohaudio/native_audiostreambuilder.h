// SPDX-License-Identifier: Apache-2.0
// Compile-only subset of the OpenHarmony OHAudio declarations used by MoL Keyboard.
#ifndef MOL_SOURCE_CHECK_NATIVE_AUDIOSTREAMBUILDER_H
#define MOL_SOURCE_CHECK_NATIVE_AUDIOSTREAMBUILDER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  AUDIOSTREAM_SUCCESS = 0,
  AUDIOSTREAM_ERROR_INVALID_PARAM = 1,
  AUDIOSTREAM_ERROR_ILLEGAL_STATE = 2,
  AUDIOSTREAM_ERROR_SYSTEM = 3
} OH_AudioStream_Result;

typedef enum { AUDIOSTREAM_TYPE_RENDERER = 1, AUDIOSTREAM_TYPE_CAPTURER = 2 } OH_AudioStream_Type;

typedef enum {
  AUDIOSTREAM_SAMPLE_U8 = 0,
  AUDIOSTREAM_SAMPLE_S16LE = 1,
  AUDIOSTREAM_SAMPLE_S24LE = 2,
  AUDIOSTREAM_SAMPLE_S32LE = 3
} OH_AudioStream_SampleFormat;

typedef enum {
  AUDIOSTREAM_ENCODING_TYPE_RAW = 0,
  AUDIOSTREAM_ENCODING_TYPE_AUDIOVIVID = 1,
  AUDIOSTREAM_ENCODING_TYPE_E_AC3 = 2
} OH_AudioStream_EncodingType;

typedef enum { AUDIOSTREAM_USAGE_UNKNOWN = 0, AUDIOSTREAM_USAGE_MUSIC = 1 } OH_AudioStream_Usage;

typedef enum {
  AUDIOSTREAM_LATENCY_MODE_NORMAL = 0,
  AUDIOSTREAM_LATENCY_MODE_FAST = 1
} OH_AudioStream_LatencyMode;

typedef enum {
  AUDIOSTREAM_INTERRUPT_FORCE = 0,
  AUDIOSTREAM_INTERRUPT_SHARE = 1
} OH_AudioInterrupt_ForceType;

typedef enum {
  AUDIOSTREAM_INTERRUPT_HINT_NONE = 0,
  AUDIOSTREAM_INTERRUPT_HINT_RESUME = 1,
  AUDIOSTREAM_INTERRUPT_HINT_PAUSE = 2,
  AUDIOSTREAM_INTERRUPT_HINT_STOP = 3,
  AUDIOSTREAM_INTERRUPT_HINT_DUCK = 4,
  AUDIOSTREAM_INTERRUPT_HINT_UNDUCK = 5
} OH_AudioInterrupt_Hint;

typedef enum {
  AUDIOSTREAM_INTERRUPT_MODE_SHARE = 0,
  AUDIOSTREAM_INTERRUPT_MODE_INDEPENDENT = 1
} OH_AudioInterrupt_Mode;

typedef enum {
  REASON_UNKNOWN = 0,
  REASON_NEW_DEVICE_AVAILABLE = 1,
  REASON_OLD_DEVICE_UNAVAILABLE = 2,
  REASON_OVERRODE = 3
} OH_AudioStream_DeviceChangeReason;

typedef enum {
  AUDIO_DATA_CALLBACK_RESULT_INVALID = -1,
  AUDIO_DATA_CALLBACK_RESULT_VALID = 0
} OH_AudioData_Callback_Result;

typedef struct OH_AudioStreamBuilderStruct OH_AudioStreamBuilder;
typedef struct OH_AudioRendererStruct OH_AudioRenderer;

typedef enum { AUDIOSTREAM_EVENT_ROUTING_CHANGED = 0 } OH_AudioStream_Event;
typedef struct OH_AudioRenderer_Callbacks_Struct {
  int32_t (*OH_AudioRenderer_OnWriteData)(OH_AudioRenderer*, void*, void*, int32_t);
  int32_t (*OH_AudioRenderer_OnStreamEvent)(OH_AudioRenderer*, void*, OH_AudioStream_Event);
  int32_t (*OH_AudioRenderer_OnInterruptEvent)(OH_AudioRenderer*, void*,
                                                OH_AudioInterrupt_ForceType,
                                                OH_AudioInterrupt_Hint);
  int32_t (*OH_AudioRenderer_OnError)(OH_AudioRenderer*, void*, OH_AudioStream_Result);
} OH_AudioRenderer_Callbacks;
typedef void (*OH_AudioRenderer_OutputDeviceChangeCallback)(
    OH_AudioRenderer* renderer, void* userData, OH_AudioStream_DeviceChangeReason reason);

OH_AudioStream_Result OH_AudioStreamBuilder_Create(OH_AudioStreamBuilder** builder,
                                                   OH_AudioStream_Type type);
OH_AudioStream_Result OH_AudioStreamBuilder_Destroy(OH_AudioStreamBuilder* builder);
OH_AudioStream_Result OH_AudioStreamBuilder_SetSamplingRate(OH_AudioStreamBuilder* builder,
                                                            int32_t rate);
OH_AudioStream_Result OH_AudioStreamBuilder_SetChannelCount(OH_AudioStreamBuilder* builder,
                                                            int32_t channelCount);
OH_AudioStream_Result OH_AudioStreamBuilder_SetSampleFormat(OH_AudioStreamBuilder* builder,
                                                            OH_AudioStream_SampleFormat format);
OH_AudioStream_Result OH_AudioStreamBuilder_SetEncodingType(
    OH_AudioStreamBuilder* builder, OH_AudioStream_EncodingType encodingType);
OH_AudioStream_Result OH_AudioStreamBuilder_SetLatencyMode(OH_AudioStreamBuilder* builder,
                                                           OH_AudioStream_LatencyMode latencyMode);
OH_AudioStream_Result OH_AudioStreamBuilder_SetRendererInfo(OH_AudioStreamBuilder* builder,
                                                            OH_AudioStream_Usage usage);
OH_AudioStream_Result OH_AudioStreamBuilder_SetRendererInterruptMode(OH_AudioStreamBuilder* builder,
                                                                     OH_AudioInterrupt_Mode mode);
OH_AudioStream_Result OH_AudioStreamBuilder_SetRendererCallback(
    OH_AudioStreamBuilder* builder, OH_AudioRenderer_Callbacks callbacks, void* userData);
OH_AudioStream_Result OH_AudioStreamBuilder_SetRendererOutputDeviceChangeCallback(
    OH_AudioStreamBuilder* builder, OH_AudioRenderer_OutputDeviceChangeCallback callback,
    void* userData);
OH_AudioStream_Result OH_AudioStreamBuilder_GenerateRenderer(OH_AudioStreamBuilder* builder,
                                                             OH_AudioRenderer** audioRenderer);

#ifdef __cplusplus
}
#endif

#endif
