#include "../include/link.hpp"
#include <cmath>
#include <iostream>

using namespace DataLinkLayer;
using namespace PhysicalLayer;

LinkLayer::LinkLayer(FramingType framing) : framing_type(framing) {}

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