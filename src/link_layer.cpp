#include "link_layer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

using PhysicalLayer::bit;
using PhysicalLayer::bytes;

namespace LinkLayer {
namespace {
constexpr uint8_t FLAG = 0x7e;
constexpr uint8_t ESC = 0x7d;
constexpr uint8_t ESC_MASK = 0x20;

std::string bits_of(const bytes &data) {
  std::string result;
  result.reserve(data.size() * 8);
  for (std::byte value : data) {
    uint8_t byte = std::to_integer<uint8_t>(value);
    for (int i = 7; i >= 0; --i)
      result.push_back((byte & (1u << i)) ? '1' : '0');
  }
  return result;
}

uint32_t crc32(const bytes &data) {
  uint32_t crc = 0xffffffffu;
  for (std::byte value : data) {
    crc ^= std::to_integer<uint8_t>(value);
    for (int i = 0; i < 8; ++i)
      crc = (crc >> 1) ^ (0xedb88320u & static_cast<uint32_t>(-(crc & 1)));
  }
  return ~crc;
}

bytes interleave(const bytes &data) {
  bytes result;
  result.reserve(data.size());
  for (size_t block = 0; block < data.size(); block += 8) {
    size_t count = std::min<size_t>(8, data.size() - block);
    if (count < 8) {
      result.insert(result.end(), data.begin() + block, data.end());
      break;
    }
    for (int column = 0; column < 8; ++column) {
      uint8_t value = 0;
      for (size_t row = 0; row < 8; ++row) {
        uint8_t source = std::to_integer<uint8_t>(data[block + row]);
        value |= ((source >> column) & 1u) << row;
      }
      result.push_back(std::byte{value});
    }
  }
  return result;
}

bytes stuff_bytes(const bytes &data) {
  bytes result{std::byte{FLAG}};
  for (std::byte value : data) {
    uint8_t byte = std::to_integer<uint8_t>(value);
    if (byte == FLAG || byte == ESC) {
      result.push_back(std::byte{ESC});
      result.push_back(std::byte{static_cast<uint8_t>(byte ^ ESC_MASK)});
    } else {
      result.push_back(value);
    }
  }
  result.push_back(std::byte{FLAG});
  return result;
}

bytes stuff_bits(const bytes &data) {
  bytes result{std::byte{FLAG}};
  int ones = 0;
  for (std::byte value : data) {
    uint8_t byte = std::to_integer<uint8_t>(value);
    for (int i = 7; i >= 0; --i) {
      bool one = (byte >> i) & 1u;
      if (one)
        ++ones;
      else
        ones = 0;
      result.push_back(std::byte{static_cast<uint8_t>(one)});
      if (ones == 5) {
        result.push_back(std::byte{0});
        ones = 0;
      }
    }
  }
  result.push_back(std::byte{FLAG});
  return result;
}

bytes unstuff_bits(const bytes &data) {
  bytes result;
  int ones = 0;
  uint8_t current = 0;
  int count = 0;
  for (std::byte value : data) {
    bool one = std::to_integer<uint8_t>(value) != 0;
    if (ones == 5 && !one) {
      ones = 0;
      continue;
    }
    ones = one ? ones + 1 : 0;
    current = static_cast<uint8_t>((current << 1) | one);
    if (++count == 8) {
      result.push_back(std::byte{current});
      current = 0;
      count = 0;
    }
  }
  if (count != 0)
    throw std::invalid_argument("quadro com quantidade de bits invalida");
  return result;
}

} // namespace

const char *framing_name(FramingType type) {
  switch (type) {
  case FramingType::CharacterCount: return "contagem";
  case FramingType::BitInterleaving: return "entrelaçamento";
  case FramingType::ByteStuffing: return "byte-stuffing";
  case FramingType::BitStuffing: return "bit-stuffing";
  }
  return "desconhecido";
}

const char *check_name(CheckType type) {
  switch (type) {
  case CheckType::None: return "nenhum";
  case CheckType::EvenParity: return "paridade";
  case CheckType::Checksum: return "checksum";
  case CheckType::CRC32: return "crc32";
  case CheckType::Hamming: return "hamming";
  }
  return "desconhecido";
}

FramingType framing_from_name(const std::string &name) {
  if (name == "contagem") return FramingType::CharacterCount;
  if (name == "entrelacamento") return FramingType::BitInterleaving;
  if (name == "byte-stuffing") return FramingType::ByteStuffing;
  if (name == "bit-stuffing") return FramingType::BitStuffing;
  throw std::invalid_argument("Enquadramento desconhecido: " + name);
}

CheckType check_from_name(const std::string &name) {
  if (name == "nenhum") return CheckType::None;
  if (name == "paridade") return CheckType::EvenParity;
  if (name == "checksum") return CheckType::Checksum;
  if (name == "crc32") return CheckType::CRC32;
  if (name == "hamming") return CheckType::Hamming;
  throw std::invalid_argument("EDC/FEC desconhecido: " + name);
}

LinkLayer::LinkLayer(Config config) : config(config) {}

void LinkLayer::set_trace_callback(TraceCallback callback) { trace = std::move(callback); }

void LinkLayer::report(const std::string &stage, const bytes &data) const {
  if (trace)
    trace(stage, bits_of(data));
}

bytes LinkLayer::add_check(const bytes &data) const {
  bytes result = data;
  switch (config.check) {
  case CheckType::None: break;
  case CheckType::EvenParity: {
    uint8_t parity = 0;
    for (std::byte value : data)
      parity ^= std::popcount(std::to_integer<uint8_t>(value)) & 1u;
    result.push_back(std::byte{parity});
    break;
  }
  case CheckType::Checksum: {
    uint8_t sum = 0;
    for (std::byte value : data) sum = static_cast<uint8_t>(sum + std::to_integer<uint8_t>(value));
    result.push_back(std::byte{static_cast<uint8_t>(~sum)});
    break;
  }
  case CheckType::CRC32: {
    uint32_t crc = crc32(data);
    for (int i = 0; i < 4; ++i)
      result.push_back(std::byte{static_cast<uint8_t>(crc >> (i * 8))});
    break;
  }
  case CheckType::Hamming: break;
  }
  return result;
}

DecodeResult LinkLayer::remove_check(const bytes &data) const {
  DecodeResult result;
  size_t check_size = config.check == CheckType::CRC32 ? 4 :
                      config.check == CheckType::None ? 0 : 1;
  if (data.size() < check_size) {
    result.error = "quadro menor que o EDC";
    return result;
  }
  bytes payload(data.begin(), data.end() - check_size);
  bytes expected = add_check(payload);
  if (!std::equal(expected.begin(), expected.end(), data.begin())) {
    result.error = "EDC invalido (" + std::string(check_name(config.check)) + ")";
    return result;
  }
  result.accepted = true;
  result.payload = std::move(payload);
  return result;
}

bytes LinkLayer::apply_hamming(const bytes &data) const {
  bytes result;
  for (std::byte value : data) {
    uint8_t input = std::to_integer<uint8_t>(value);
    std::array<uint8_t, 13> code{};
    for (int position : {3, 5, 6, 7, 9, 10, 11, 12})
      code[position] = input & 1u, input >>= 1;
    for (int parity : {1, 2, 4, 8}) {
      uint8_t value_parity = 0;
      for (int position = 1; position <= 12; ++position)
        if ((position & parity) && position != parity) value_parity ^= code[position];
      code[parity] = value_parity;
    }
    uint16_t packed = 0;
    for (int position = 1; position <= 12; ++position) packed |= code[position] << (position - 1);
    result.push_back(std::byte{static_cast<uint8_t>(packed)});
    result.push_back(std::byte{static_cast<uint8_t>(packed >> 8)});
  }
  return result;
}

DecodeResult LinkLayer::remove_hamming(const bytes &data) const {
  DecodeResult result;
  if (data.size() % 2 != 0) {
    result.error = "Hamming truncado";
    return result;
  }
  for (size_t i = 0; i < data.size(); i += 2) {
    uint16_t packed = std::to_integer<uint8_t>(data[i]) |
                      (std::to_integer<uint8_t>(data[i + 1]) << 8);
    std::array<uint8_t, 13> code{};
    for (int position = 1; position <= 12; ++position) code[position] = (packed >> (position - 1)) & 1u;
    int syndrome = 0;
    for (int parity : {1, 2, 4, 8}) {
      uint8_t parity_value = 0;
      for (int position = 1; position <= 12; ++position)
        if (position & parity) parity_value ^= code[position];
      if (parity_value) syndrome += parity;
    }
    if (syndrome > 0 && syndrome <= 12) {
      code[syndrome] ^= 1u;
      result.corrected = true;
    }
    uint8_t value = 0;
    int shift = 0;
    for (int position : {3, 5, 6, 7, 9, 10, 11, 12}) value |= code[position] << shift++;
    result.payload.push_back(std::byte{value});
  }
  result.accepted = true;
  return result;
}

bytes LinkLayer::apply_framing(const bytes &data) const {
  switch (config.framing) {
  case FramingType::CharacterCount: {
    bytes result{std::byte{static_cast<uint8_t>(data.size() >> 8)}, std::byte{static_cast<uint8_t>(data.size())}};
    result.insert(result.end(), data.begin(), data.end());
    return result;
  }
  case FramingType::BitInterleaving: {
    bytes result{std::byte{static_cast<uint8_t>(data.size() >> 8)},
                 std::byte{static_cast<uint8_t>(data.size())}};
    bytes interleaved = interleave(data);
    result.insert(result.end(), interleaved.begin(), interleaved.end());
    return result;
  }
  case FramingType::ByteStuffing: return stuff_bytes(data);
  case FramingType::BitStuffing: return stuff_bits(data);
  }
  return data;
}

bytes LinkLayer::remove_framing(const bytes &data) const {
  switch (config.framing) {
  case FramingType::CharacterCount:
    if (data.size() < 2 || (static_cast<size_t>(std::to_integer<uint8_t>(data[0])) << 8 |
                            std::to_integer<uint8_t>(data[1])) != data.size() - 2)
      throw std::invalid_argument("contagem de caracteres invalida");
    return bytes(data.begin() + 2, data.end());
  case FramingType::BitInterleaving:
    if (data.size() < 2 ||
        (static_cast<size_t>(std::to_integer<uint8_t>(data[0])) << 8 |
         std::to_integer<uint8_t>(data[1])) != data.size() - 2)
      throw std::invalid_argument("tamanho do entrelacamento invalido");
    return interleave(bytes(data.begin() + 2, data.end()));
  case FramingType::ByteStuffing: return data;
  case FramingType::BitStuffing: return unstuff_bits(data);
  }
  return data;
}

bytes LinkLayer::encode(const bytes &payload) {
  if (payload.size() > config.max_frame_size) throw std::invalid_argument("payload excede o tamanho maximo");
  report("app-tx", payload);
  bytes checked = config.check == CheckType::Hamming ? apply_hamming(payload) : add_check(payload);
  report("enlace-tx", checked);
  bytes frame = apply_framing(checked);
  report("quadro-tx", frame);
  return frame;
}

DecodeResult LinkLayer::decode(const bytes &frame) {
  DecodeResult result;
  try {
    bytes checked = remove_framing(frame);
    report("quadro-rx", checked);
    result = config.check == CheckType::Hamming ? remove_hamming(checked) : remove_check(checked);
    if (result.accepted) {
      if (result.payload.size() > config.max_frame_size) {
        result.accepted = false;
        result.error = "payload excede o tamanho maximo";
      } else {
        report("app-rx", result.payload);
      }
    }
  } catch (const std::exception &error) {
    result.error = error.what();
  }
  return result;
}

void LinkLayer::push(const bytes &chunk, const std::function<void(const DecodeResult &)> &callback) {
  if (config.framing == FramingType::ByteStuffing || config.framing == FramingType::BitStuffing) {
    for (std::byte value : chunk) {
      uint8_t byte = std::to_integer<uint8_t>(value);
      if (config.framing == FramingType::ByteStuffing) {
        if (byte == FLAG) {
          if (!stream.empty()) callback(decode(stream));
          stream.clear();
          escaping = false;
        } else if (escaping) {
          stream.push_back(std::byte{static_cast<uint8_t>(byte ^ ESC_MASK)});
          escaping = false;
        } else if (byte == ESC) escaping = true;
        else stream.push_back(value);
      } else {
        if (byte == FLAG) {
          if (!stream.empty()) callback(decode(stream));
          stream.clear();
        } else stream.push_back(value);
      }
    }
    return;
  }
  stream.insert(stream.end(), chunk.begin(), chunk.end());
    if ((config.framing == FramingType::CharacterCount ||
      config.framing == FramingType::BitInterleaving) && stream.size() >= 2) {
    size_t size = (std::to_integer<uint8_t>(stream[0]) << 8) | std::to_integer<uint8_t>(stream[1]);
    if (stream.size() >= size + 2) {
      bytes frame(stream.begin(), stream.begin() + size + 2);
      stream.erase(stream.begin(), stream.begin() + size + 2);
      callback(decode(frame));
    }
  } else if (config.framing == FramingType::BitInterleaving && !stream.empty()) {
    callback(decode(stream));
    stream.clear();
  }
}

void LinkLayer::reset() {
  stream.clear();
  escaping = false;
}

} // namespace LinkLayer
