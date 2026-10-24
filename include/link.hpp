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

enum ErrorCorrectionType {
    NONE_CORRECTION,
    HAMMING
};

class LinkLayer {
public:
    LinkLayer(FramingType framing, ErrorDetectionType detection = NONE_DETECTION, ErrorCorrectionType correction = NONE_CORRECTION);

    PhysicalLayer::bytes pack(const PhysicalLayer::bytes &payload);
    bool unpack(const PhysicalLayer::bytes &frame, PhysicalLayer::bytes &payload);

private:
    FramingType framing_type;
    ErrorDetectionType detection_type;
    ErrorCorrectionType correction_type;

    // Enquadramento
    PhysicalLayer::bytes apply_framing(const PhysicalLayer::bytes &payload);
    PhysicalLayer::bytes remove_framing(const PhysicalLayer::bytes &frame, bool &ok);

    // Controle de Erro: Detecção
    PhysicalLayer::bytes apply_error_detection(const PhysicalLayer::bytes &data);
    bool verify_error_detection(PhysicalLayer::bytes &data);

    // Controle de Erro: Correção (Hamming)
    PhysicalLayer::bytes apply_error_correction(const PhysicalLayer::bytes &data);
    PhysicalLayer::bytes verify_and_correct_error(const PhysicalLayer::bytes &data, bool &ok);
    uint8_t encode_hamming74(uint8_t data);
    uint8_t decode_hamming74(uint8_t encoded);

    // Auxiliares
    std::vector<uint8_t> bytes_to_bits(const PhysicalLayer::bytes &data);
    PhysicalLayer::bytes bits_to_bytes(const std::vector<uint8_t> &bits);
};

} // namespace DataLinkLayer

#endif