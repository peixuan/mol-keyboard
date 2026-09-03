// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#include "harmony_ohaudio_sim.h"
#include "oh_audio_host.h"

namespace {

#define CHECK(condition)                                                           \
  do {                                                                             \
    if (!(condition)) {                                                            \
      std::cerr << "check failed at line " << __LINE__ << ": " #condition << '\n'; \
      return false;                                                                \
    }                                                                              \
  } while (false)

using mol::harmony::AudioHost;
using mol::harmony::test::ApiConfiguration;

constexpr std::int32_t kStereoFrameBytes = 4;

bool contains_signal(const std::vector<std::int16_t>& samples) {
  return std::any_of(samples.begin(), samples.end(),
                     [](std::int16_t sample) { return sample != 0; });
}

bool render_frames(std::vector<std::int16_t>& samples) {
  const auto byte_count = static_cast<std::int32_t>(samples.size() * sizeof(samples[0]));
  return mol::harmony::test::write(samples.data(), byte_count) == AUDIO_DATA_CALLBACK_RESULT_VALID;
}

bool test_fast_path_render_route_and_recovery() {
  ApiConfiguration configuration{};
  configuration.underflow_count = 7U;
  mol::harmony::test::configure(configuration);

  auto host = std::make_unique<AudioHost>();
  CHECK(host->start() == AUDIOSTREAM_SUCCESS);
  const auto api = mol::harmony::test::snapshot();
  CHECK(api.builder_creates == 1U);
  CHECK(api.builder_destroys == 1U);
  CHECK(api.renderer_generates == 1U);
  CHECK(api.renderer_starts == 1U);
  CHECK(api.requested_sample_rate == 48000);
  CHECK(api.requested_channel_count == 2);
  CHECK(api.requested_sample_format == AUDIOSTREAM_SAMPLE_S16LE);
  CHECK(api.requested_encoding == AUDIOSTREAM_ENCODING_TYPE_RAW);
  CHECK(api.requested_latency == AUDIOSTREAM_LATENCY_MODE_FAST);
  CHECK(api.requested_usage == AUDIOSTREAM_USAGE_MUSIC);
  CHECK(api.requested_interrupt_mode == AUDIOSTREAM_INTERRUPT_MODE_SHARE);
  CHECK(api.renderer_callbacks_configured);
  CHECK(api.device_change_callback_configured);
  CHECK(api.renderer_alive);

  auto status = host->status();
  CHECK(status.active);
  CHECK(!status.needs_restart);
  CHECK(status.fast_path_active);
  CHECK(!status.latency_fallback_used);
  CHECK(status.sample_rate == 48000);
  CHECK(status.frame_size == 192);
  CHECK(status.underflow_count == 7U);

  CHECK(host->note_on(60U, 0.8F, 91U) == MOL_OK);
  std::vector<std::int16_t> audio(640U * 2U, 0);
  CHECK(render_frames(audio));
  CHECK(contains_signal(audio));
  status = host->status();
  CHECK(status.callback_count == 1U);
  CHECK(status.rendered_frames == 640U);
  CHECK(status.render_failures == 0U);
  CHECK(status.non_finite_samples == 0U);
  CHECK(host->note_off(60U, 91U) == MOL_OK);

  std::array<std::uint8_t, 3U> malformed{{1U, 2U, 3U}};
  CHECK(mol::harmony::test::write(malformed.data(), static_cast<std::int32_t>(malformed.size())) ==
        AUDIO_DATA_CALLBACK_RESULT_VALID);
  CHECK(std::all_of(malformed.begin(), malformed.end(),
                    [](std::uint8_t value) { return value == 0U; }));
  CHECK(host->status().render_failures == 1U);

  mol::harmony::test::change_output_device(REASON_OLD_DEVICE_UNAVAILABLE);
  status = host->status();
  CHECK(!status.active);
  CHECK(status.needs_restart);
  CHECK(status.route_changes == 1U);
  std::fill(audio.begin(), audio.end(), static_cast<std::int16_t>(123));
  CHECK(render_frames(audio));
  CHECK(!contains_signal(audio));

  CHECK(host->recover() == AUDIOSTREAM_SUCCESS);
  status = host->status();
  CHECK(status.active);
  CHECK(!status.needs_restart);
  CHECK(status.route_changes == 1U);
  const auto recovered_api = mol::harmony::test::snapshot();
  CHECK(recovered_api.renderer_starts == 2U);
  CHECK(recovered_api.renderer_stops == 1U);
  CHECK(recovered_api.renderer_releases == 1U);

  host->stop();
  status = host->status();
  CHECK(!status.active);
  CHECK(!status.needs_restart);
  CHECK(status.sample_rate == 0);
  CHECK(status.frame_size == 0);
  CHECK(!mol::harmony::test::snapshot().renderer_alive);
  CHECK(mol::harmony::test::write(audio.data(), kStereoFrameBytes) ==
        AUDIO_DATA_CALLBACK_RESULT_INVALID);
  return true;
}

bool test_latency_fallback_and_start_failures() {
  ApiConfiguration configuration{};
  configuration.reject_fast_latency = true;
  mol::harmony::test::configure(configuration);
  {
    auto host = std::make_unique<AudioHost>();
    CHECK(host->start() == AUDIOSTREAM_SUCCESS);
    const auto status = host->status();
    CHECK(status.active);
    CHECK(!status.fast_path_active);
    CHECK(status.latency_fallback_used);
    CHECK(status.latency_mode == AUDIOSTREAM_LATENCY_MODE_NORMAL);
    const auto api = mol::harmony::test::snapshot();
    CHECK(api.builder_creates == 2U);
    CHECK(api.builder_destroys == 2U);
    CHECK(api.renderer_generates == 2U);
    CHECK(api.renderer_starts == 1U);
  }

  configuration = ApiConfiguration{};
  configuration.reported_channel_count = 1;
  mol::harmony::test::configure(configuration);
  {
    auto host = std::make_unique<AudioHost>();
    CHECK(host->start() == AUDIOSTREAM_ERROR_INVALID_PARAM);
    const auto status = host->status();
    CHECK(!status.active);
    CHECK(status.needs_restart);
    CHECK(status.last_error == AUDIOSTREAM_ERROR_INVALID_PARAM);
    CHECK(mol::harmony::test::snapshot().renderer_releases == 1U);
  }

  configuration = ApiConfiguration{};
  configuration.start_result = AUDIOSTREAM_ERROR_SYSTEM;
  mol::harmony::test::configure(configuration);
  {
    auto host = std::make_unique<AudioHost>();
    CHECK(host->start() == AUDIOSTREAM_ERROR_SYSTEM);
    const auto status = host->status();
    CHECK(!status.active);
    CHECK(status.needs_restart);
    CHECK(status.last_error == AUDIOSTREAM_ERROR_SYSTEM);
    const auto api = mol::harmony::test::snapshot();
    CHECK(api.renderer_starts == 1U);
    CHECK(api.renderer_stops == 1U);
    CHECK(api.renderer_releases == 1U);
  }

  configuration = ApiConfiguration{};
  configuration.generate_result = AUDIOSTREAM_ERROR_ILLEGAL_STATE;
  mol::harmony::test::configure(configuration);
  {
    auto host = std::make_unique<AudioHost>();
    CHECK(host->start() == AUDIOSTREAM_ERROR_ILLEGAL_STATE);
    const auto status = host->status();
    CHECK(!status.active);
    CHECK(status.needs_restart);
    const auto api = mol::harmony::test::snapshot();
    CHECK(api.builder_creates == 2U);
    CHECK(api.builder_destroys == 2U);
    CHECK(api.renderer_generates == 2U);
    CHECK(api.renderer_starts == 0U);
  }
  return true;
}

bool test_interrupt_and_error_recovery() {
  mol::harmony::test::reset();
  auto host = std::make_unique<AudioHost>();
  CHECK(host->start() == AUDIOSTREAM_SUCCESS);

  CHECK(mol::harmony::test::interrupt(AUDIOSTREAM_INTERRUPT_SHARE,
                                      AUDIOSTREAM_INTERRUPT_HINT_PAUSE) == 0);
  auto status = host->status();
  CHECK(status.active);
  CHECK(!status.needs_restart);
  CHECK(status.interruptions == 1U);

  CHECK(mol::harmony::test::interrupt(AUDIOSTREAM_INTERRUPT_FORCE,
                                      AUDIOSTREAM_INTERRUPT_HINT_DUCK) == 0);
  status = host->status();
  CHECK(status.active);
  CHECK(status.interruptions == 2U);

  CHECK(mol::harmony::test::interrupt(AUDIOSTREAM_INTERRUPT_FORCE,
                                      AUDIOSTREAM_INTERRUPT_HINT_PAUSE) == 0);
  status = host->status();
  CHECK(!status.active);
  CHECK(status.needs_restart);
  CHECK(status.interruptions == 3U);
  CHECK(host->recover() == AUDIOSTREAM_SUCCESS);

  CHECK(mol::harmony::test::interrupt(AUDIOSTREAM_INTERRUPT_FORCE,
                                      AUDIOSTREAM_INTERRUPT_HINT_RESUME) == 0);
  status = host->status();
  CHECK(status.active);
  CHECK(status.needs_restart);
  CHECK(host->recover() == AUDIOSTREAM_SUCCESS);

  CHECK(mol::harmony::test::fail(AUDIOSTREAM_ERROR_SYSTEM) == 0);
  status = host->status();
  CHECK(!status.active);
  CHECK(status.needs_restart);
  CHECK(status.last_error == AUDIOSTREAM_ERROR_SYSTEM);
  CHECK(host->recover() == AUDIOSTREAM_SUCCESS);
  CHECK(host->status().active);
  CHECK(host->status().last_error == AUDIOSTREAM_SUCCESS);
  return true;
}

bool test_commands_recording_and_playback() {
  mol::harmony::test::reset();
  auto host = std::make_unique<AudioHost>();
  CHECK(host->start() == AUDIOSTREAM_SUCCESS);
  CHECK(host->note_on(128U, 0.5F, 1U) == MOL_ERROR_INVALID_ARGUMENT);
  CHECK(host->note_on(60U, std::numeric_limits<float>::quiet_NaN(), 1U) ==
        MOL_ERROR_INVALID_ARGUMENT);
  CHECK(host->submit_control(std::numeric_limits<std::uint32_t>::max(), 0U, 0, 0, 0, 0, 0.0F,
                             0.0F) == MOL_ERROR_INVALID_ARGUMENT);

  std::vector<std::int16_t> audio(256U * 2U, 0);
  CHECK(host->submit_control(MOL_COMMAND_RECORD_START, 0U, 0, 0, 0, 0, 0.0F, 0.0F) == MOL_OK);
  CHECK(host->note_on(64U, 0.75F, 440U) == MOL_OK);
  CHECK(render_frames(audio));
  CHECK(contains_signal(audio));
  CHECK(host->note_off(64U, 440U) == MOL_OK);
  CHECK(render_frames(audio));
  CHECK(host->submit_control(MOL_COMMAND_RECORD_STOP, 0U, 0, 0, 0, 0, 0.0F, 0.0F) == MOL_OK);
  CHECK(render_frames(audio));

  std::vector<std::uint8_t> sequence(2U * 1024U * 1024U, 0U);
  std::size_t sequence_size = 0U;
  CHECK(host->export_recording(sequence.data(), sequence.size(), &sequence_size) == MOL_OK);
  CHECK(sequence_size > 0U);
  CHECK(sequence_size < sequence.size());
  CHECK(host->export_recording(nullptr, sequence.size(), &sequence_size) ==
        MOL_ERROR_INVALID_ARGUMENT);

  host->stop();
  CHECK(host->start() == AUDIOSTREAM_SUCCESS);
  CHECK(host->load_recording(sequence.data(), sequence_size) == MOL_OK);
  CHECK(host->load_recording(sequence.data(), 1U) != MOL_OK);
  CHECK(host->submit_control(MOL_COMMAND_PLAYBACK_START, 0U, 0, 0, 0, 0, 0.0F, 0.0F) == MOL_OK);
  bool playback_signal = false;
  for (std::uint32_t iteration = 0U; iteration < 8U; ++iteration) {
    std::fill(audio.begin(), audio.end(), static_cast<std::int16_t>(0));
    CHECK(render_frames(audio));
    playback_signal = playback_signal || contains_signal(audio);
  }
  CHECK(playback_signal);
  return true;
}

}  // namespace

int main() {
  if (!test_fast_path_render_route_and_recovery() || !test_latency_fallback_and_start_failures() ||
      !test_interrupt_and_error_recovery() || !test_commands_recording_and_playback()) {
    return 1;
  }
  std::cout << "HarmonyOS OHAudio production-host simulation passed\n";
  return 0;
}
