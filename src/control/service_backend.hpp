// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_CONTROL_SERVICE_BACKEND_HPP_
#define MOL_CONTROL_SERVICE_BACKEND_HPP_

#include <filesystem>
#include <string_view>

#include "service_rpc.hpp"
#include "service_runtime.hpp"

namespace molcontrol {

class ServiceBackend final : public RpcBackend {
 public:
  ServiceBackend(ServiceRuntime& runtime, std::filesystem::path state_directory);

  molseq::Json invoke(std::string_view method, const molseq::Json& params) override;
  [[nodiscard]] const std::filesystem::path& recordings_directory() const noexcept {
    return recordings_directory_;
  }

 private:
  molseq::Json invoke_checked(std::string_view method, const molseq::Json& params);
  void persist_config() const;
  molseq::Json config_;
  ServiceRuntime& runtime_;
  std::filesystem::path state_directory_;
  std::filesystem::path recordings_directory_;
  std::filesystem::path config_path_;
};

}  // namespace molcontrol

#endif  // MOL_CONTROL_SERVICE_BACKEND_HPP_
