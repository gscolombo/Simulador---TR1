#ifndef LINK_LAYER_H
#define LINK_LAYER_H

#include "types.hpp"
#include <cstddef>
#include <functional>
#include <string>

namespace LinkLayer {

enum class FramingType { CharacterCount, BitInterleaving, ByteStuffing, BitStuffing };
enum class CheckType { None, EvenParity, Checksum, CRC32, Hamming };

struct Config {
  FramingType framing = FramingType::ByteStuffing;
  CheckType check = CheckType::CRC32;
  size_t max_frame_size = 256;
};

struct DecodeResult {
  bool accepted = false;
  bool corrected = false;
  std::string error;
  PhysicalLayer::bytes payload;
};

using TraceCallback = std::function<void(const std::string &, const std::string &)>;

const char *framing_name(FramingType type);
const char *check_name(CheckType type);
FramingType framing_from_name(const std::string &name);
CheckType check_from_name(const std::string &name);

/** Codifica e decodifica quadros da camada de enlace. */
class LinkLayer {
public:
  explicit LinkLayer(Config config = {});

  void set_trace_callback(TraceCallback callback);
  PhysicalLayer::bytes encode(const PhysicalLayer::bytes &payload);
  DecodeResult decode(const PhysicalLayer::bytes &frame);

  // Consome um fluxo recebido e chama callback para cada quadro completo.
  void push(const PhysicalLayer::bytes &chunk,
            const std::function<void(const DecodeResult &)> &callback);
  void reset();

private:
  Config config;
  TraceCallback trace;
  PhysicalLayer::bytes stream;
  bool escaping = false;

  PhysicalLayer::bytes add_check(const PhysicalLayer::bytes &data) const;
  DecodeResult remove_check(const PhysicalLayer::bytes &data) const;
  PhysicalLayer::bytes apply_framing(const PhysicalLayer::bytes &data) const;
  PhysicalLayer::bytes remove_framing(const PhysicalLayer::bytes &data) const;
  PhysicalLayer::bytes apply_hamming(const PhysicalLayer::bytes &data) const;
  DecodeResult remove_hamming(const PhysicalLayer::bytes &data) const;
  void report(const std::string &stage, const PhysicalLayer::bytes &data) const;
};

} // namespace LinkLayer

#endif
