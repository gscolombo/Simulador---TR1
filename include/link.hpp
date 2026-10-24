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

enum ErrorDetectionType {
    NONE_DETECTION,
    PARITY_EVEN,
    CHECKSUM,
    CRC32
};

class LinkLayer {
public:
    // Agora o construtor recebe também o tipo de detecção (default: sem detecção)
    LinkLayer(FramingType framing, ErrorDetectionType detection = NONE_DETECTION);

    // Métodos principais que integram Erro + Enquadramento
    PhysicalLayer::bytes pack(const PhysicalLayer::bytes &payload);
    bool unpack(const PhysicalLayer::bytes &frame, PhysicalLayer::bytes &payload);

private:
    FramingType framing_type;
    ErrorDetectionType detection_type;

    // Enquadramento
    PhysicalLayer::bytes apply_framing(const PhysicalLayer::bytes &payload);
    PhysicalLayer::bytes remove_framing(const PhysicalLayer::bytes &frame, bool &ok);

    // Controle de Erro (Novas funções)
    PhysicalLayer::bytes apply_error_detection(const PhysicalLayer::bytes &data);
    bool verify_error_detection(PhysicalLayer::bytes &data);

    // Auxiliares
    std::vector<uint8_t> bytes_to_bits(const PhysicalLayer::bytes &data);
    PhysicalLayer::bytes bits_to_bytes(const std::vector<uint8_t> &bits);
};

} // namespace DataLinkLayer

#endif