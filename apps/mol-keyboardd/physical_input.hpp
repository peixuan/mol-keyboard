// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_KEYBOARDD_PHYSICAL_INPUT_HPP_
#define MOL_KEYBOARDD_PHYSICAL_INPUT_HPP_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "service_runtime.hpp"

namespace molkeyboardd {

class PhysicalInputAdapter {
 public:
  using CommandSink = std::function<mol_result_t(const mol_command_t&)>;

  virtual ~PhysicalInputAdapter() = default;
  virtual std::vector<molcontrol::DeviceInfo> devices() = 0;
  virtual mol_result_t attach(const std::string& id, CommandSink sink) = 0;
  virtual void detach() = 0;
  virtual std::string active_id() const = 0;
};

std::unique_ptr<PhysicalInputAdapter> make_physical_input_adapter();

}  // namespace molkeyboardd

#endif  // MOL_KEYBOARDD_PHYSICAL_INPUT_HPP_
