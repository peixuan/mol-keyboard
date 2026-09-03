// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_KEYBOARDD_MIDI_INPUT_LINUX_TEST_HPP_
#define MOL_KEYBOARDD_MIDI_INPUT_LINUX_TEST_HPP_

#include <filesystem>
#include <memory>

#include "physical_input.hpp"

namespace molkeyboardd {

std::unique_ptr<PhysicalInputAdapter> make_linux_midi_input_adapter_for_test(
    const std::filesystem::path& directory);

}  // namespace molkeyboardd

#endif  // MOL_KEYBOARDD_MIDI_INPUT_LINUX_TEST_HPP_
