#ifndef LINK_HPP
#define LINK_HPP

#include "types.hpp"
#include <vector>
#include <cstdint>
#include <cstddef>

namespace DataLinkLayer {

enum FramingType {
    CHAR_COUNT,
    BIT_INTERLEAVING,
    BYTE_STUFFING,
    BIT_STUFFING
};

class LinkLayer {
public:
    LinkLayer(FramingType framing);

    // Empacota os dados da aplicação em um quadro (Transmissor)
    PhysicalLayer::bytes apply_framing(const PhysicalLayer::bytes &payload);

    // Desempacota o quadro recebido, recuperando o payload original (Receptor)
    PhysicalLayer::bytes remove_framing(const PhysicalLayer::bytes &frame, bool &ok);

private:
    FramingType framing_type;

    // Métodos auxiliares para bits (úteis para Bit Stuffing e Interleaving)
    std::vector<uint8_t> bytes_to_bits(const PhysicalLayer::bytes &data);
    PhysicalLayer::bytes bits_to_bytes(const std::vector<uint8_t> &bits);
};

} // namespace DataLinkLayer

#endif