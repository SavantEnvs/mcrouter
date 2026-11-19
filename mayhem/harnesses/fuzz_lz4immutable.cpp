// Fuzz Lz4Immutable::decompress — mcrouter's own LZ4 block decoder (with an immutable dictionary),
// used to decompress backend replies (Lz4ImmutableCompressionCodec). The compressed bytes come
// from the network, so they are untrusted.
//
// Input layout:
//   byte 0      mode: bit0 = 1 -> round-trip (compress the payload, decompress, compare)
//   bytes 1-3   uncompressedSize (little endian, 24 bits)
//   bytes 4-5   split points: the payload is decompressed as up to 3 iovecs, each in its own
//               exact-size heap buffer, so a read past any piece is caught by ASan.
//   rest        payload (compressed data, or plaintext in round-trip mode)
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include <sys/uio.h>

#include <folly/io/IOBuf.h>

#include "mcrouter/lib/Lz4Immutable.h"

using facebook::memcache::Lz4Immutable;

namespace {
const Lz4Immutable& codec() {
  // Fixed, deterministic 16 KiB dictionary of memcache-reply-like text (read-only across inputs).
  static const Lz4Immutable* c = [] {
    std::string dict;
    uint32_t x = 0x12345678;
    while (dict.size() < 16 * 1024) {
      dict += "VALUE key";
      x = x * 1103515245u + 12345u;
      dict += std::to_string(x % 1000);
      dict += " 0 10\r\n0123456789\r\nEND\r\n";
    }
    return new Lz4Immutable(folly::IOBuf::copyBuffer(dict));
  }();
  return *c;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 6) {
    return -1;
  }
  const bool roundTrip = data[0] & 1;
  size_t uncompressedSize = data[1] | (data[2] << 8) | (size_t(data[3]) << 16);
  const uint8_t* payload = data + 6;
  size_t n = size - 6;

  std::vector<uint8_t> compressed;
  if (roundTrip) {
    auto out = codec().compress(*folly::IOBuf::wrapBuffer(payload, n));
    out->coalesce();
    compressed.assign(out->data(), out->data() + out->length());
    payload = compressed.data();
    n = compressed.size();
    uncompressedSize = size - 6;
  }

  size_t s1 = n ? data[4] % (n + 1) : 0;
  size_t s2 = n ? s1 + data[5] % (n - s1 + 1) : 0;
  size_t cuts[4] = {0, s1, s2, n};
  std::vector<std::unique_ptr<uint8_t[]>> pieces;
  std::vector<iovec> iov;
  for (int i = 0; i < 3; ++i) {
    size_t len = cuts[i + 1] - cuts[i];
    std::unique_ptr<uint8_t[]> p(new uint8_t[len ? len : 1]);
    if (len) {
      std::memcpy(p.get(), payload + cuts[i], len);
    }
    iov.push_back({p.get(), len});
    pieces.push_back(std::move(p));
  }

  auto res = codec().decompress(iov.data(), iov.size(), uncompressedSize);
  if (roundTrip) {
    // compress() output must always decompress back to the original bytes.
    if (!res) {
      abort();
    }
    res->coalesce();
    if (res->length() != size - 6 ||
        (res->length() && std::memcmp(res->data(), data + 6, size - 6) != 0)) {
      abort();
    }
  } else if (res) {
    res->coalesce();
  }
  return 0;
}
