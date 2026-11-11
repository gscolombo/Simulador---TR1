#include "main.hpp"
#include "link_layer.hpp"

#include <array>
#include <cstddef>

using namespace LinkLayer;
using PhysicalLayer::bytes;

namespace {
bytes payload() {
  return {std::byte{'A'}, std::byte{0x7e}, std::byte{0x7d}, std::byte{'Z'}};
}
}

TEST_CASE("Todos os enquadramentos preservam o payload") {
  for (FramingType framing : {FramingType::CharacterCount,
                              FramingType::BitInterleaving,
                              FramingType::ByteStuffing,
                              FramingType::BitStuffing}) {
    LinkLayer::LinkLayer link({framing, CheckType::CRC32, 64});
    DecodeResult result;
    bool received = false;
    link.push(link.encode(payload()), [&](const DecodeResult &candidate) {
      result = candidate;
      received = true;
    });
    REQUIRE(received);
    CHECK(result.accepted);
    CHECK(result.payload == payload());
  }
}

TEST_CASE("Hamming corrige um erro simples") {
  LinkLayer::LinkLayer link({FramingType::CharacterCount, CheckType::Hamming, 64});
  bytes frame = link.encode(payload());
  frame[2] ^= std::byte{1};
  DecodeResult result = link.decode(frame);
  CHECK(result.accepted);
  CHECK(result.corrected);
  CHECK(result.payload == payload());
}

TEST_CASE("O decodificador aceita quadro fragmentado") {
  LinkLayer::LinkLayer link({FramingType::ByteStuffing, CheckType::EvenParity, 64});
  bytes frame = link.encode(payload());
  DecodeResult result;
  bool received = false;
  for (std::byte value : frame)
    link.push(bytes{value}, [&](const DecodeResult &candidate) {
      result = candidate;
      received = true;
    });
  REQUIRE(received);
  CHECK(result.accepted);
  CHECK(result.payload == payload());
}
