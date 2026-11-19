// Fuzz mcrouter's client-side reply parser: the bytes a memcache backend server sends back to
// mcrouter. ClientMcParser parses ASCII replies (ragel McClientAsciiParser) or Caret replies
// (caretParseHeader + Carbon reply deserialization), and Caret bodies whose header names a
// compression codec are decompressed first — here codec id 1 is an Lz4Immutable codec (the
// codec mcrouter's OSS build keeps), so compressed replies reach Lz4Immutable::decompress.
//
// Input:
//   byte 0   read-chunk size - 1 (1..256 bytes per readDataAvailable)
//   byte 1   bit0: protocol (0 = ASCII, 1 = Caret)
//   bytes 2-9  reply-type script: each time the parser asks for the next reply, the next byte
//              (cycling) picks which request type the reply answers (as McClientRequestContext
//              would, from the request that was sent; only types the ASCII reply parser
//              supports, McClientAsciiParser::initializeReplyParser)
//   rest     the server byte stream
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>

#include <folly/io/IOBuf.h>
#include <folly/io/async/EventBase.h>

#include "mcrouter/lib/CompressionCodecManager.h"
#include "mcrouter/lib/network/ClientMcParser.h"
#include "mcrouter/lib/network/gen/MemcacheMessages.h"

using namespace facebook::memcache;

namespace {

const CompressionCodecMap* codecMap() {
  static folly::EventBase* evb = new folly::EventBase();
  static CompressionCodecManager* mgr = [] {
    std::string dict;
    while (dict.size() < 8 * 1024) {
      dict += "VALUE key 0 10\r\n0123456789\r\nEND\r\n";
    }
    std::unordered_map<uint32_t, CodecConfigPtr> cfgs;
    cfgs.emplace(1, std::make_unique<CodecConfig>(1, CompressionCodecType::LZ4Immutable, dict));
    return new CompressionCodecManager(std::move(cfgs));
  }();
  return mgr->getCodecMap(*evb);
}

size_t gSink = 0;

class Callback {
 public:
  using Parser = ClientMcParser<Callback>;
  Parser* parser{nullptr};
  const uint8_t* script{nullptr};
  size_t scriptPos{0};
  bool failed{false};

  template <class Reply>
  void replyReady(Reply&& reply, uint64_t, RpcStatsContext) {
    gSink += static_cast<size_t>(*reply.result_ref());
    if constexpr (requires { carbon::valueRangeSlow(reply); }) {
      gSink += carbon::valueRangeSlow(reply).size();
    }
  }
  void parseError(carbon::Result, folly::StringPiece) {
    failed = true;
  }
  void handleConnectionControlMessage(const CaretMessageInfo&) {}
  bool nextReplyAvailable(uint64_t) {
    uint8_t sel = script[scriptPos++ % 8];
    switch (sel % 16) {
      case 0: parser->expectNext<McGetRequest>(); break;
      case 1: parser->expectNext<McGetsRequest>(); break;
      case 2: parser->expectNext<McSetRequest>(); break;
      case 3: parser->expectNext<McDeleteRequest>(); break;
      case 4: parser->expectNext<McLeaseGetRequest>(); break;
      case 5: parser->expectNext<McLeaseSetRequest>(); break;
      case 6: parser->expectNext<McIncrRequest>(); break;
      case 7: parser->expectNext<McMetagetRequest>(); break;
      case 8: parser->expectNext<McVersionRequest>(); break;
      case 9: parser->expectNext<McPrependRequest>(); break;
      case 10: parser->expectNext<McCasRequest>(); break;
      case 11: parser->expectNext<McTouchRequest>(); break;
      case 12: parser->expectNext<McGatRequest>(); break;
      case 13: parser->expectNext<McGatsRequest>(); break;
      case 14: parser->expectNext<McAppendRequest>(); break;
      default: parser->expectNext<McFlushAllRequest>(); break;
    }
    return true;
  }
};

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 10) {
    return -1;
  }
  size_t chunk = size_t(data[0]) + 1;
  mc_protocol_t proto = (data[1] & 1) ? mc_caret_protocol : mc_ascii_protocol;
  Callback cb;
  cb.script = data + 2;
  data += 10;
  size -= 10;

  Callback::Parser parser(cb, 4096, 4096 * 16, false, codecMap());
  cb.parser = &parser;
  parser.setProtocol(proto);
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
