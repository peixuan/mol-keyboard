// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_CONTROL_SERVICE_RPC_HPP_
#define MOL_CONTROL_SERVICE_RPC_HPP_

#include <array>
#include <string>
#include <string_view>

#include "jsonrpc.hpp"

namespace molcontrol {

inline constexpr std::array<std::string_view, 41> kRequiredRpcMethods = {
    "system.getInfo",
    "system.getCapabilities",
    "system.getMetrics",
    "system.shutdown",
    "engine.getState",
    "engine.reset",
    "engine.allNotesOff",
    "engine.allSoundOff",
    "preset.list",
    "preset.select",
    "preset.getParameters",
    "preset.setParameter",
    "transport.get",
    "transport.setTempo",
    "transport.setTimeSignature",
    "transport.start",
    "transport.stop",
    "input.listDevices",
    "input.attach",
    "input.detach",
    "input.getMapping",
    "input.setMapping",
    "audio.listDevices",
    "audio.selectDevice",
    "audio.getLatency",
    "performance.noteOn",
    "performance.noteOff",
    "performance.control",
    "recording.start",
    "recording.stop",
    "recording.list",
    "recording.load",
    "recording.save",
    "playback.start",
    "playback.stop",
    "playback.seek",
    "config.get",
    "config.set",
    "diagnostics.selfTest",
    "diagnostics.doctor",
    "diagnostics.benchmark",
};

class RpcBackend {
 public:
  virtual ~RpcBackend() = default;
  virtual molseq::Json invoke(std::string_view method, const molseq::Json& params) = 0;
};

bool register_service_methods(JsonRpcDispatcher& dispatcher, RpcBackend& backend);

}  // namespace molcontrol

#endif  // MOL_CONTROL_SERVICE_RPC_HPP_
