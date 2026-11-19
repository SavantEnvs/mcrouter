// Fuzz mcrouter's server-side request parser: the bytes a memcache client sends to mcrouter.
// ServerMcParser detects the protocol from the first byte (ASCII memcache via the ragel
// McServerAsciiParser, or Caret binary: caretParseHeader), and Caret request bodies are
// deserialized into the Memcache Carbon request structs exactly as McServerSession does it
// (CarbonMessageDispatcher<MemcacheRequestList> -> CarbonProtocolReader::readRawInto).
//
// Input: byte 0 picks the read-chunk size (1..256 bytes per readDataAvailable, as a socket would
// deliver it); the rest is the client byte stream.
#include <cstdint>
#include <cstring>
#include <utility>

#include <folly/io/IOBuf.h>

#include "mcrouter/lib/network/CarbonMessageDispatcher.h"
#include "mcrouter/lib/network/ServerMcParser.h"
#include "mcrouter/lib/network/gen/MemcacheServer.h"

using namespace facebook::memcache;

namespace {

size_t gSink = 0;

template <class Request>
void consume(const Request& req) {
  if constexpr (requires { req.key_ref()->fullKey(); }) {
    gSink += req.key_ref()->fullKey().size();
  }
}

class CaretProc : public CarbonMessageDispatcher<
                      detail::MemcacheRequestList,
                      CaretProc> {
 public:
  template <class M>
  void onTypedMessage(const CaretMessageInfo&, const folly::IOBuf&, M&& req) {
    consume(req);
  }
};

class Callback {
 public:
  bool failed{false};
  CaretProc proc;

  void caretRequestReady(const CaretMessageInfo& header, const folly::IOBuf& buf) {
    proc.dispatchTypedRequest(header, buf);
  }
  void parseError(carbon::Result, folly::StringPiece) {
    failed = true;
  }
  template <class Request>
  void onRequest(Request&& req, bool) {
    consume(req);
  }
  void multiOpEnd() {}
};

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) {
    return -1;
  }
  size_t chunk = size_t(data[0]) + 1;
  data++;
  size--;

  Callback cb;
  ServerMcParser<Callback> parser(cb, 4096, 4096 * 16);
  while (size > 0 && !cb.failed) {
    auto buf = parser.getReadBuffer();
    size_t n = std::min({buf.second, size, chunk});
    if (n == 0) {
      break;
    }
    std::memcpy(buf.first, data, n);
    data += n;
    size -= n;
    if (!parser.readDataAvailable(n)) {
      break;
    }
  }
  return 0;
}
