// Fuzz ConfigPreprocessor::getConfigWithoutMacros — the JSONC + macro language mcrouter applies
// to every routing config it loads (--config file:..., or a config pushed by a config source).
// The input is the config text; @import resolves to a fixed in-memory set (no files).
#include <cstdint>
#include <exception>
#include <string>

#include <folly/Range.h>
#include <folly/json/json.h>

#include "mcrouter/lib/config/ConfigPreprocessor.h"
#include "mcrouter/lib/config/ImportResolverIf.h"

using facebook::memcache::ConfigPreprocessor;
using facebook::memcache::ImportResolverIf;

namespace {
class FixedImportResolver : public ImportResolverIf {
  std::string import(folly::StringPiece path) final {
    if (path == "macro") {
      return R"({ "type": "macroDef", "params": ["a"], "result": "%a%" })";
    }
    if (path == "pool") {
      return R"({ "servers": ["localhost:5000", "localhost:5001"] })";
    }
    if (path == "list") {
      return R"([1, 2, "three", {"k": "v"}])";
    }
    throw std::runtime_error("cannot import");
  }
};
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FixedImportResolver resolver;
  folly::json::metadata_map metadata;
  folly::F14NodeMap<std::string, folly::dynamic> globals = {
      {"service", "memcache"}, {"region", "test"}};
  try {
    auto res = ConfigPreprocessor::getConfigWithoutMacros(
        folly::StringPiece(reinterpret_cast<const char*>(data), size),
        resolver,
        std::move(globals),
        &metadata);
    (void)folly::toJson(res);
  } catch (const std::exception&) {
  }
  return 0;
}
