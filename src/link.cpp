#include "../include/link.hpp"
#include <cmath>
#include <iostream>

using namespace DataLinkLayer;
using namespace PhysicalLayer;

LinkLayer::LinkLayer(FramingType framing, ErrorDetectionType detection, ErrorCorrectionType correction) 
    : framing_type(framing), detection_type(detection), correction_type(correction) {}

// ==========================================
// MÉTODOS AUXILIARES DE CONVERSÃO BIT/BYTE
// ==========================================
std::vector<uint8_t> LinkLayer::bytes_to_bits(const bytes &data) {
    std::vector<uint8_t> bits;
    for (auto b : data) {
        int val = std::to_integer<int>(b);
        for (int i = 7; i >= 0; --i) {
            bits.push_back((val >> i) & 1);
        }
    }
    return bits;
}

bytes LinkLayer::bits_to_bytes(const std::vector<uint8_t> &bits) {
    bytes data;
    int current_byte = 0;
    int bit_count = 0;
    
    for (uint8_t bit : bits) {
        current_byte = (current_byte << 1) | (bit & 1);
        bit_count++;
        if (bit_count == 8) {
            data.push_back(static_cast<std::byte>(current_byte));
            current_byte = 0;
            bit_count = 0;
        }
    }
    // Preenchimento (padding) caso não feche 8 bits
    if (bit_count > 0) {
        current_byte <<= (8 - bit_count);
        data.push_back(static_cast<std::byte>(current_byte));
    }
    return data;
}

// ==========================================
// ENQUADRAMENTO - TRANSMISSOR
// ==========================================
bytes LinkLayer::apply_framing(const bytes &payload) {
    bytes framed;

    switch (framing_type) {
        case CHAR_COUNT: {
            // O tamanho total inclui o próprio byte de contagem
            uint8_t size = static_cast<uint8_t>(payload.size() + 1);
            framed.push_back(static_cast<std::byte>(size));
            framed.insert(framed.end(), payload.begin(), payload.end());
            break;
        }

        case BYTE_STUFFING: {
            std::byte FLAG{0x7E};
            std::byte ESC{0x7D};
            
            framed.push_back(FLAG);
            for (auto b : payload) {
                if (b == FLAG || b == ESC) {
                    framed.push_back(ESC);
                    framed.push_back(b ^ std::byte{0x20}); // Mascara o byte
                } else {
                    framed.push_back(b);
                }
            }
            framed.push_back(FLAG);
            break;
        }

        case BIT_STUFFING: {
            std::vector<uint8_t> bits = bytes_to_bits(payload);
            std::vector<uint8_t> stuffed_bits;
            
            // Flag de início: 01111110
            std::vector<uint8_t> flag = {0,1,1,1,1,1,1,0};
            stuffed_bits.insert(stuffed_bits.end(), flag.begin(), flag.end());
            
            int consecutive_ones = 0;
            for (uint8_t bit : bits) {
                stuffed_bits.push_back(bit);
                if (bit == 1) {
                    consecutive_ones++;
                    if (consecutive_ones == 5) {
                        stuffed_bits.push_back(0); // Inserção de bit
                        consecutive_ones = 0;
                    }
                } else {
                    consecutive_ones = 0;
                }
            }
            
            // Flag de fim
            stuffed_bits.insert(stuffed_bits.end(), flag.begin(), flag.end());
            framed = bits_to_bytes(stuffed_bits);
            break;
        }

        case BIT_INTERLEAVING: {
            // Simulação de matriz de entrelaçamento 8x8 (blocos de 8 bytes)
            std::vector<uint8_t> bits = bytes_to_bits(payload);
            std::vector<uint8_t> interleaved_bits;
            
            size_t block_size = 64; // 8 bytes = 64 bits
            // Pad para garantir blocos completos
            while (bits.size() % block_size != 0) bits.push_back(0);

            for (size_t block = 0; block < bits.size(); block += block_size) {
                // Escreve linha a linha, lê coluna a coluna
                for (size_t col = 0; col < 8; ++col) {
                    for (size_t row = 0; row < 8; ++row) {
                        interleaved_bits.push_back(bits[block + (row * 8) + col]);
                    }
                }
            }
            framed = bits_to_bytes(interleaved_bits);
            break;
        }
    }
    return framed;
}

// ==========================================
// DESENQUADRAMENTO - RECEPTOR
// ==========================================
bytes LinkLayer::remove_framing(const bytes &frame, bool &ok) {
    ok = true;
    bytes payload;

    switch (framing_type) {
        case CHAR_COUNT: {
            if (frame.empty()) { ok = false; return {}; }
            size_t size = std::to_integer<size_t>(frame[0]);
            
            if (frame.size() < size) { ok = false; return {}; } // Quadro incompleto
            payload.assign(frame.begin() + 1, frame.begin() + size);
            break;
        }

        case BYTE_STUFFING: {
            if (frame.size() < 2) { ok = false; return {}; }
            std::byte ESC{0x7D};

            // Ignora o primeiro e o último byte (Flags)
            for (size_t i = 1; i < frame.size() - 1; ++i) {
                if (frame[i] == ESC) {
                    if (i + 1 < frame.size() - 1) {
                        payload.push_back(frame[++i] ^ std::byte{0x20});
                    } else {
                        ok = false; // Escape pendurado no final
                    }
                } else {
                    payload.push_back(frame[i]);
                }
            }
            break;
        }

        case BIT_STUFFING: {
            std::vector<uint8_t> bits = bytes_to_bits(frame);
            std::vector<uint8_t> destuffed_bits;
            
            int consecutive_ones = 0;
            bool inside_frame = false;

            for (size_t i = 0; i < bits.size(); ++i) {
                // Verifica a flag (01111110)
                if (i + 7 < bits.size()) {
                    if (bits[i]==0 && bits[i+1]==1 && bits[i+2]==1 && 
                        bits[i+3]==1 && bits[i+4]==1 && bits[i+5]==1 && 
                        bits[i+6]==1 && bits[i+7]==0) {
                        
                        inside_frame = !inside_frame;
                        i += 7; // Pula a flag
                        consecutive_ones = 0;
                        continue;
                    }
                }
                
                if (inside_frame) {
                    destuffed_bits.push_back(bits[i]);
                    if (bits[i] == 1) {
                        consecutive_ones++;
                        if (consecutive_ones == 5) {
                            // Se o próximo bit for 0, é um bit inserido (stuffing), então o ignoramos.
                            if (i + 1 < bits.size() && bits[i + 1] == 0) {
                                i++; // Pula o bit '0' inserido
                            }
                            consecutive_ones = 0;
                        }
                    } else {
                        consecutive_ones = 0;
                    }
                }
            }
            payload = bits_to_bytes(destuffed_bits);
            break;
        }

        case BIT_INTERLEAVING: {
            std::vector<uint8_t> bits = bytes_to_bits(frame);
            std::vector<uint8_t> deinterleaved_bits;
            size_t block_size = 64;

            if (bits.size() % block_size != 0) { ok = false; return {}; }

            for (size_t block = 0; block < bits.size(); block += block_size) {
                // Lê coluna a coluna, reescreve linha a linha (inverso do transmissor)
                for (size_t col = 0; col < 8; ++col) {
                    for (size_t row = 0; row < 8; ++row) {
                        deinterleaved_bits.push_back(bits[block + (row * 8) + col]);
                    }
                }
            }
            
            // Note que o padding do final pode introduzir zeros (NULL chars)
            payload = bits_to_bytes(deinterleaved_bits);
            break;
        }
    }
    return payload;
}

bytes LinkLayer::pack(const bytes &payload) {
    bytes detected_data = apply_error_detection(payload);
    bytes corrected_data = apply_error_correction(detected_data);
    return apply_framing(corrected_data);
}

bool LinkLayer::unpack(const bytes &frame, bytes &payload) {
    bool ok = false;
    bytes unframed_data = remove_framing(frame, ok);
    if (!ok) return false;

    bytes uncorrected_data = verify_and_correct_error(unframed_data, ok);
    if (!ok) return false;

    payload = uncorrected_data;
    return verify_error_detection(payload); // CRC decide a palavra final
}

// ==========================================
// CONTROLE DE ERROS: DETECÇÃO
// ==========================================
bytes LinkLayer::apply_error_detection(const bytes &data) {
    bytes result = data;

    if (detection_type == PARITY_EVEN) {
        int ones = 0;
        for (auto b : data) {
            uint8_t val = static_cast<uint8_t>(b);
            for (int i = 0; i < 8; i++) {
                ones += ((val >> i) & 1);
            }
        }
        // Adiciona 1 byte no final (0x01 se ímpar, 0x00 se par)
        uint8_t parity = (ones % 2 != 0) ? 1 : 0;
        result.push_back(static_cast<std::byte>(parity));
    } 
    else if (detection_type == CHECKSUM) {
        uint32_t sum = 0;
        // Soma blocos de 16 bits
        for (size_t i = 0; i < data.size(); i += 2) {
            uint16_t word = (static_cast<uint8_t>(data[i]) << 8);
            if (i + 1 < data.size()) {
                word |= static_cast<uint8_t>(data[i + 1]);
            }
            sum += word;
        }
        // Fold do carry de 32 para 16 bits
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        uint16_t checksum = ~sum;
        // Adiciona os 2 bytes do checksum
        result.push_back(static_cast<std::byte>(checksum >> 8));
        result.push_back(static_cast<std::byte>(checksum & 0xFF));
    }
    else if (detection_type == CRC32) {
        uint32_t crc = 0xFFFFFFFF;
        for (auto b : data) {
            crc ^= static_cast<uint8_t>(b);
            for (int j = 0; j < 8; j++) {
                if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
                else crc >>= 1;
            }
        }
        crc = ~crc;
        // Adiciona os 4 bytes do CRC-32
        result.push_back(static_cast<std::byte>((crc >> 24) & 0xFF));
        result.push_back(static_cast<std::byte>((crc >> 16) & 0xFF));
        result.push_back(static_cast<std::byte>((crc >> 8) & 0xFF));
        result.push_back(static_cast<std::byte>(crc & 0xFF));
    }
    
    return result;
}

bool LinkLayer::verify_error_detection(bytes &data) {
    if (detection_type == NONE_DETECTION) return true;

    if (detection_type == PARITY_EVEN) {
        if (data.empty()) return false;
        uint8_t received_parity = static_cast<uint8_t>(data.back());
        data.pop_back(); // Remove o byte de paridade da carga útil

        int ones = 0;
        for (auto b : data) {
            uint8_t val = static_cast<uint8_t>(b);
            for (int i = 0; i < 8; i++) {
                ones += ((val >> i) & 1);
            }
        }
        uint8_t calculated = (ones % 2 != 0) ? 1 : 0;
        return received_parity == calculated;
    }
    else if (detection_type == CHECKSUM) {
        if (data.size() < 2) return false;
        size_t n = data.size();
        uint16_t received_checksum = (static_cast<uint8_t>(data[n-2]) << 8) | 
                                      static_cast<uint8_t>(data[n-1]);
        data.resize(n - 2);

        uint32_t sum = 0;
        for (size_t i = 0; i < data.size(); i += 2) {
            uint16_t word = (static_cast<uint8_t>(data[i]) << 8);
            if (i + 1 < data.size()) {
                word |= static_cast<uint8_t>(data[i + 1]);
            }
            sum += word;
        }
        while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
        uint16_t calculated = ~sum;
        
        return received_checksum == calculated;
    }
    else if (detection_type == CRC32) {
        if (data.size() < 4) return false;
        size_t n = data.size();
        uint32_t received_crc = (static_cast<uint32_t>(data[n-4]) << 24) |
                                (static_cast<uint32_t>(data[n-3]) << 16) |
                                (static_cast<uint32_t>(data[n-2]) << 8)  |
                                (static_cast<uint32_t>(data[n-1]));
        data.resize(n - 4);

        uint32_t crc = 0xFFFFFFFF;
        for (auto b : data) {
            crc ^= static_cast<uint8_t>(b);
            for (int j = 0; j < 8; j++) {
                if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
                else crc >>= 1;
            }
        }
        crc = ~crc;
        
        return received_crc == crc;
    }
    return true;
}

uint8_t LinkLayer::encode_hamming74(uint8_t data) {
    // Isola os 4 bits de dados (d1, d2, d3, d4)
    uint8_t d1 = (data >> 3) & 1;
    uint8_t d2 = (data >> 2) & 1;
    uint8_t d3 = (data >> 1) & 1;
    uint8_t d4 = data & 1;

    // Calcula os 3 bits de paridade par
    uint8_t p1 = d1 ^ d2 ^ d4;
    uint8_t p2 = d1 ^ d3 ^ d4;
    uint8_t p4 = d2 ^ d3 ^ d4;

    // Posiciona os bits nas casas corretas (de 1 a 7) para o cálculo da síndrome funcionar perfeitamente depois. O bit 0 fica livre.
    return (p1 << 1) | (p2 << 2) | (d1 << 3) | (p4 << 4) | (d2 << 5) | (d3 << 6) | (d4 << 7);
}

uint8_t LinkLayer::decode_hamming74(uint8_t encoded) {
    uint8_t p1 = (encoded >> 1) & 1;
    uint8_t p2 = (encoded >> 2) & 1;
    uint8_t d1 = (encoded >> 3) & 1;
    uint8_t p4 = (encoded >> 4) & 1;
    uint8_t d2 = (encoded >> 5) & 1;
    uint8_t d3 = (encoded >> 6) & 1;
    uint8_t d4 = (encoded >> 7) & 1;

    // Calcula a Síndrome de erro
    uint8_t s1 = p1 ^ d1 ^ d2 ^ d4;
    uint8_t s2 = p2 ^ d1 ^ d3 ^ d4;
    uint8_t s4 = p4 ^ d2 ^ d3 ^ d4;

    uint8_t syndrome = (s4 << 2) | (s2 << 1) | s1;

    if (syndrome != 0) {
        // Se síndrome > 0, ocorreu um erro na posição correspondente. 
        // Inverte o bit exato para corrigi-lo.
        encoded ^= (1 << syndrome); 
        
        // Re-extrai os dados agora que o bit foi corrigido
        d1 = (encoded >> 3) & 1;
        d2 = (encoded >> 5) & 1;
        d3 = (encoded >> 6) & 1;
        d4 = (encoded >> 7) & 1;
    }

    return (d1 << 3) | (d2 << 2) | (d3 << 1) | d4;
}

bytes LinkLayer::apply_error_correction(const bytes &data) {
    if (correction_type == NONE_CORRECTION) return data;

    bytes result;
    if (correction_type == HAMMING) {
        for (auto b : data) {
            uint8_t val = static_cast<uint8_t>(b);
            // Quebra 1 byte em dois pedaços de 4 bits
            uint8_t high_nibble = (val >> 4) & 0x0F;
            uint8_t low_nibble = val & 0x0F;

            // Insere os dois bytes codificados com Hamming(7,4)
            result.push_back(static_cast<std::byte>(encode_hamming74(high_nibble)));
            result.push_back(static_cast<std::byte>(encode_hamming74(low_nibble)));
        }
    }
    return result;
}

bytes LinkLayer::verify_and_correct_error(const bytes &data, bool &ok) {
    ok = true;
    if (correction_type == NONE_CORRECTION) return data;

    bytes result;
    if (correction_type == HAMMING) {
        if (data.size() % 2 != 0) {
            ok = false; // Quadro quebrado
            return result;
        }
        for (size_t i = 0; i < data.size(); i += 2) {
            uint8_t high_encoded = static_cast<uint8_t>(data[i]);
            uint8_t low_encoded = static_cast<uint8_t>(data[i+1]);

            // Decodifica e corrige eventuais falhas (flip de 1 bit)
            uint8_t high_nibble = decode_hamming74(high_encoded);
            uint8_t low_nibble = decode_hamming74(low_encoded);

            // Remonta o byte original
            uint8_t val = (high_nibble << 4) | low_nibble;
            result.push_back(static_cast<std::byte>(val));
        }
    }
    return result;
}