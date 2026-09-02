// SPDX-License-Identifier: Apache-2.0
// Compile-only subset of the OpenHarmony OHAudio declarations used by MoL Keyboard.
#ifndef MOL_SOURCE_CHECK_NATIVE_AUDIORENDERER_H
#define MOL_SOURCE_CHECK_NATIVE_AUDIORENDERER_H

#include "native_audiostreambuilder.h"

#ifdef __cplusplus
extern "C" {
#endif

OH_AudioStream_Result OH_AudioRenderer_Release(OH_AudioRenderer* renderer);
OH_AudioStream_Result OH_AudioRenderer_Start(OH_AudioRenderer* renderer);
OH_AudioStream_Result OH_AudioRenderer_Stop(OH_AudioRenderer* renderer);
OH_AudioStream_Result OH_AudioRenderer_GetSamplingRate(OH_AudioRenderer* renderer, int32_t* rate);
OH_AudioStream_Result OH_AudioRenderer_GetChannelCount(OH_AudioRenderer* renderer,
                                                       int32_t* channelCount);
OH_AudioStream_Result OH_AudioRenderer_GetSampleFormat(OH_AudioRenderer* renderer,
                                                       OH_AudioStream_SampleFormat* sampleFormat);
OH_AudioStream_Result OH_AudioRenderer_GetLatencyMode(OH_AudioRenderer* renderer,
                                                      OH_AudioStream_LatencyMode* latencyMode);
OH_AudioStream_Result OH_AudioRenderer_GetFrameSizeInCallback(OH_AudioRenderer* renderer,
                                                              int32_t* frameSize);
OH_AudioStream_Result OH_AudioRenderer_GetUnderflowCount(OH_AudioRenderer* renderer,
                                                         uint32_t* count);

#ifdef __cplusplus
}
#endif

#endif
