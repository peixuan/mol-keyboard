// SPDX-License-Identifier: Apache-2.0
#include <cstdio>
#include <stdexcept>
#include <string>

#include "jsonrpc.hpp"

namespace {

int failures;

#define EXPECT_TRUE(condition)                                                                 \
  do {                                                                                         \
    if (!(condition)) {                                                                        \
      std::fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                              \
    }                                                                                          \
  } while (false)

int error_code(const std::string& response) {
  const molseq::Json root = molseq::parse_json(response);
  return static_cast<int>(molseq::json_i64(
      molseq::require_member(molseq::require_member(root, "error"), "code"), -32768, 32767));
}

void test_requests() {
  molcontrol::JsonRpcDispatcher dispatcher;
  int notifications = 0;
  EXPECT_TRUE(dispatcher.add_method("echo", [](const molseq::Json& params) { return params; }));
  EXPECT_TRUE(dispatcher.add_method("notify", [&notifications](const molseq::Json&) {
    ++notifications;
    return molseq::Json{};
  }));
  EXPECT_TRUE(!dispatcher.add_method("echo", [](const molseq::Json&) { return molseq::Json{}; }));
  const std::string response =
      dispatcher.dispatch(R"({"jsonrpc":"2.0","method":"echo","params":{"value":7},"id":"a"})");
  const molseq::Json root = molseq::parse_json(response);
  EXPECT_TRUE(molseq::json_string(molseq::require_member(root, "id")) == "a");
  EXPECT_TRUE(molseq::json_u64(
                  molseq::require_member(molseq::require_member(root, "result"), "value"), 10u) ==
              7u);
  EXPECT_TRUE(dispatcher.dispatch(R"({"jsonrpc":"2.0","method":"notify"})").empty());
  EXPECT_TRUE(notifications == 1);
  const std::string batch = dispatcher.dispatch(
      R"([{"jsonrpc":"2.0","method":"echo","params":[1],"id":1},{"jsonrpc":"2.0","method":"missing","id":2},{"jsonrpc":"2.0","method":"notify"}])");
  const molseq::Json batch_root = molseq::parse_json(batch);
  EXPECT_TRUE(batch_root.type == molseq::Json::Type::Array && batch_root.array.size() == 2u);
  EXPECT_TRUE(notifications == 2);
}

void test_errors_and_bounds() {
  molcontrol::JsonRpcDispatcher dispatcher;
  EXPECT_TRUE(dispatcher.add_method("invalid", [](const molseq::Json&) -> molseq::Json {
    throw molcontrol::RpcError(-32602, "Expected a value");
  }));
  EXPECT_TRUE(dispatcher.add_method("failure", [](const molseq::Json&) -> molseq::Json {
    throw std::runtime_error("private detail");
  }));
  EXPECT_TRUE(error_code(dispatcher.dispatch("{")) == -32700);
  EXPECT_TRUE(error_code(dispatcher.dispatch(R"({"jsonrpc":"1.0","method":"x","id":1})")) ==
              -32600);
  EXPECT_TRUE(error_code(dispatcher.dispatch(
                  R"({"jsonrpc":"2.0","method":"x","extension":true,"id":1})")) == -32600);
  EXPECT_TRUE(error_code(dispatcher.dispatch(R"({"jsonrpc":"2.0","method":"x","id":1})")) ==
              -32601);
  EXPECT_TRUE(error_code(dispatcher.dispatch(R"({"jsonrpc":"2.0","method":"invalid","id":1})")) ==
              -32602);
  const std::string internal =
      dispatcher.dispatch(R"({"jsonrpc":"2.0","method":"failure","id":1})");
  EXPECT_TRUE(error_code(internal) == -32603 &&
              internal.find("private detail") == std::string::npos);
  EXPECT_TRUE(error_code(dispatcher.dispatch("[]")) == -32600);
  EXPECT_TRUE(error_code(dispatcher.dispatch(
                  std::string(molcontrol::kMaxRpcRequestBytes + 1u, ' '))) == -32600);
}

}  // namespace

int main() {
  test_requests();
  test_errors_and_bounds();
  if (failures != 0) {
    std::fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
