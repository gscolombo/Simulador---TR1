#include "link_layer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

using PhysicalLayer::bit;
using PhysicalLayer::bytes;

namespace LinkLayer {
namespace {
// Definição de constantes globais para os protocolos baseados em flags (PPP / HDLC)
constexpr uint8_t FLAG = 0x7e;      // Delimitador de início e fim de quadro (01111110)
constexpr uint8_t ESC = 0x7d;       // Caractere de escape para Byte Stuffing
constexpr uint8_t ESC_MASK = 0x20;  // Máscara XOR (bit 5) aplicada no Byte Stuffing

/**
 * @brief Converte um vetor de bytes em uma string legível contendo seus bits ('0' e '1').
 * Utilizado pelo sistema de rastreamento (trace/report) para depuração visual.
 */
std::string bits_of(const bytes &data) {
  std::string result;
  result.reserve(data.size() * 8);
  for (std::byte value : data) {
    uint8_t byte = std::to_integer<uint8_t>(value);
    // Percorre cada bit do byte, do mais significativo (7) ao menos significativo (0)
    for (int i = 7; i >= 0; --i)
      result.push_back((byte & (1u << i)) ? '1' : '0');
  }
  return result;
}

/**
 * @brief Calcula o CRC-32 (Cyclic Redundancy Check) utilizando o padrão IEEE 802.
 */
uint32_t crc32(const bytes &data) {
  uint32_t crc = 0xffffffffu;
  for (std::byte value : data) {
    crc ^= std::to_integer<uint8_t>(value);
    for (int i = 0; i < 8; ++i)
      crc = (crc >> 1) ^ (0xedb88320u & static_cast<uint32_t>(-(crc & 1)));
  }
  return ~crc;
}

/**
 * @brief Realiza o entrelaçamento de bits (Bit Interleaving) organizando blocos de 8 bytes
 * em matrizes lógicas e transpondo suas linhas e colunas.
 */
bytes interleave(const bytes &data) {
  bytes result;
  result.reserve(data.size());
  // Processa os dados em blocos de 8 bytes
  for (size_t block = 0; block < data.size(); block += 8) {
    size_t count = std::min<size_t>(8, data.size() - block);
    // Se o bloco final for menor que 8 bytes, copia sem entrelaçar
    if (count < 8) {
      result.insert(result.end(), data.begin() + block, data.end());
      break;
    }
    // Transposição matricial: percorre cada coluna para formar um novo byte
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

/**
 * @brief Aplica Byte Stuffing (PPP) inserindo Flags nas pontas e escapando conflitos de FLAG e ESC.
 */
bytes stuff_bytes(const bytes &data) {
  bytes result{std::byte{FLAG}}; // Insere a Flag de abertura
  for (std::byte value : data) {
    uint8_t byte = std::to_integer<uint8_t>(value);
    // Se o byte coincidir com os caracteres de controle, aplica escape e máscara XOR
    if (byte == FLAG || byte == ESC) {
      result.push_back(std::byte{ESC});
      result.push_back(std::byte{static_cast<uint8_t>(byte ^ ESC_MASK)});
    } else {
      result.push_back(value);
    }
  }
  result.push_back(std::byte{FLAG}); // Insere a Flag de fechamento
  return result;
}

/**
 * @brief Aplica Bit Stuffing: insere um bit '0' após cada sequência de 5 bits '1' consecutivos.
 */
bytes stuff_bits(const bytes &data) {
  bytes result{std::byte{FLAG}}; // Inicia com a Flag de abertura
  int ones = 0, bits = 0;
  uint16_t current = 0;
  for (std::byte value : data) {
    uint8_t byte = std::to_integer<uint8_t>(value);
    for (int i = 7; i >= 0; --i) {
      bool one = (byte >> i) & 1u;
      if (one)
        ++ones;
      else
        ones = 0;

      bits++;

      current = (current << 1u) | one;

      // Regra dos 5 uns: injeta um '0' artificialmente
      if (ones == 5) {
        bits++;
        current <<= 1u;
        current |= 0;
        ones = 0;
      }

      if (bits >= 8) {
        result.push_back(
            std::byte{static_cast<uint8_t>((current >> (bits - 8)) & 0x00FF)});
        bits -= 8;
      }
    }
  }
  if (bits > 0)
    result.push_back(
        std::byte{static_cast<uint8_t>((current << (8 - bits)) & (0x00FF))});
  result.push_back(std::byte{FLAG}); // Finaliza com a Flag de fechamento
  return result;
}

/**
 * @brief Remove o Bit Stuffing (Bit Unstuffing) retirando os bits '0' enxertados pelo transmissor.
 */
bytes unstuff_bits(const bytes &data) {
  bytes result;
  int ones = 0;
  uint8_t current = 0;
  int count = 0;
  for (std::byte value : data) {
    uint8_t byte = std::to_integer<uint8_t>(value);

    for (int i = 7; i >= 0; i--) {
      bool one = (byte >> i) & 1u;
      if (one)
        ones++;
      else {
        // Se encontrou 5 uns seguidos seguidos de um '0', remove o '0' (ignora)
        bool stuff = ones == 5;
        ones = 0;
        if (stuff)
          continue;
      }

      current = (current << 1u) | one;

      // Agrupa 8 bits recuperados para formar 1 byte completo
      if (++count == 8) {
        result.push_back(std::byte{current});
        count = 0;
      }
    }
  }
  return result;
}

} // namespace

/**
 * @brief Converte o Enum de Enquadramento para sua representação em texto.
 */
const char *framing_name(FramingType type) {
  switch (type) {
  case FramingType::CharacterCount: return "contagem";
  case FramingType::BitInterleaving: return "entrelaçamento";
  case FramingType::ByteStuffing: return "byte-stuffing";
  case FramingType::BitStuffing: return "bit-stuffing";
  }
  return "desconhecido";
}

/**
 * @brief Converte o Enum de Verificação de Erros (EDC/FEC) para sua representação em texto.
 */
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

/**
 * @brief Converte uma string de texto para o Enum correspondente de Enquadramento.
 */
FramingType framing_from_name(const std::string &name) {
  if (name == "contagem") return FramingType::CharacterCount;
  if (name == "entrelacamento") return FramingType::BitInterleaving;
  if (name == "byte-stuffing") return FramingType::ByteStuffing;
  if (name == "bit-stuffing") return FramingType::BitStuffing;
  throw std::invalid_argument("Enquadramento desconhecido: " + name);
}

/**
 * @brief Converte uma string de texto para o Enum correspondente de EDC/FEC.
 */
CheckType check_from_name(const std::string &name) {
  if (name == "nenhum") return CheckType::None;
  if (name == "paridade") return CheckType::EvenParity;
  if (name == "checksum") return CheckType::Checksum;
  if (name == "crc32") return CheckType::CRC32;
  if (name == "hamming") return CheckType::Hamming;
  throw std::invalid_argument("EDC/FEC desconhecido: " + name);
}

// Construtor da classe LinkLayer
LinkLayer::LinkLayer(Config config) : config(config) {}

// Registra a função de callback para telemetria/rastreamento de quadros
void LinkLayer::set_trace_callback(TraceCallback callback) { trace = std::move(callback); }

// Dispara o relatório de rastreamento convertendo bytes para string binária
void LinkLayer::report(const std::string &stage, const bytes &data) const {
  if (trace)
    trace(stage, bits_of(data));
}

/**
 * @brief Calcula e anexa o código de detecção de erros (EDC) escolhido ao final do payload.
 */
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
    result.push_back(std::byte{static_cast<uint8_t>(~sum)}); // Complemento de 1
    break;
  }
  case CheckType::CRC32: {
    uint32_t crc = crc32(data);
    // Fatia o CRC de 32 bits em 4 bytes individuais e os anexa
    for (int i = 0; i < 4; ++i)
      result.push_back(std::byte{static_cast<uint8_t>(crc >> (i * 8))});
    break;
  }
  case CheckType::Hamming: break; // Tratado separadamente em apply_hamming
  }
  return result;
}

/**
 * @brief Valida o EDC recebido e separa o payload em caso de sucesso.
 */
DecodeResult LinkLayer::remove_check(const bytes &data) const {
  DecodeResult result;
  size_t check_size = config.check == CheckType::CRC32 ? 4 :
                      config.check == CheckType::None ? 0 : 1;
  if (data.size() < check_size) {
    result.error = "quadro menor que o EDC";
    return result;
  }
  // Separa dados úteis do código de verificação
  bytes payload(data.begin(), data.end() - check_size);
  bytes expected = add_check(payload);
  // Compara o EDC recalculado com o EDC que veio anexado
  if (!std::equal(expected.begin(), expected.end(), data.begin())) {
    result.error = "EDC invalido (" + std::string(check_name(config.check)) + ")";
    return result;
  }
  result.accepted = true;
  result.payload = std::move(payload);
  return result;
}

/**
 * @brief Aplica o Código de Hamming(12,8) FEC: expande cada byte em 12 bits protegidos por paridade.
 */
bytes LinkLayer::apply_hamming(const bytes &data) const {
  bytes result;
  for (std::byte value : data) {
    uint8_t input = std::to_integer<uint8_t>(value);
    std::array<uint8_t, 13> code{};
    // Posiciona os bits de dados nas posições que não são potências de 2 (3, 5, 6, 7, 9, 10, 11, 12)
    for (int position : {3, 5, 6, 7, 9, 10, 11, 12})
      code[position] = input & 1u, input >>= 1;

    // Calcula os bits de paridade nas posições 1, 2, 4 e 8
    for (int parity : {1, 2, 4, 8}) {
      uint8_t value_parity = 0;
      for (int position = 1; position <= 12; ++position)
        if ((position & parity) && position != parity) value_parity ^= code[position];
      code[parity] = value_parity;
    }
    // Empacota os 12 bits em uma palavra de 16 bits
    uint16_t packed = 0;
    for (int position = 1; position <= 12; ++position) packed |= code[position] << (position - 1);

    // Divide os 16 bits em 2 bytes e armazena no resultado
    result.push_back(std::byte{static_cast<uint8_t>(packed)});
    result.push_back(std::byte{static_cast<uint8_t>(packed >> 8)});
  }
  return result;
}

/**
 * @brief Remove o Hamming, calcula a síndrome, corrige erros simples de 1 bit (FEC) e extrai o payload.
 */
DecodeResult LinkLayer::remove_hamming(const bytes &data) const {
  DecodeResult result;
  if (data.size() % 2 != 0) {
    result.error = "Hamming truncado";
    return result;
  }
  // Processa em blocos de 2 bytes
  for (size_t i = 0; i < data.size(); i += 2) {
    uint16_t packed = std::to_integer<uint8_t>(data[i]) |
                      (std::to_integer<uint8_t>(data[i + 1]) << 8);
    std::array<uint8_t, 13> code{};
    for (int position = 1; position <= 12; ++position) code[position] = (packed >> (position - 1)) & 1u;

    // Calcula a síndrome para detectar falhas
    int syndrome = 0;
    for (int parity : {1, 2, 4, 8}) {
      uint8_t parity_value = 0;
      for (int position = 1; position <= 12; ++position)
        if (position & parity) parity_value ^= code[position];
      if (parity_value) syndrome += parity;
    }
    // Se houver erro detectado (síndrome válida), corrige o bit invertendo seu valor
    if (syndrome > 0 && syndrome <= 12) {
      code[syndrome] ^= 1u;
      result.corrected = true;
    }
    // Extrai de volta o byte original das posições de dados
    uint8_t value = 0;
    int shift = 0;
    for (int position : {3, 5, 6, 7, 9, 10, 11, 12}) value |= code[position] << shift++;
    result.payload.push_back(std::byte{value});
  }
  result.accepted = true;
  return result;
}

/**
 * @brief Encapsula os dados protegidos aplicando o protocolo de enquadramento configurado (Transmissor).
 */
bytes LinkLayer::apply_framing(const bytes &data) const {
  switch (config.framing) {
  case FramingType::CharacterCount: {
    // Insere cabeçalho de 2 bytes indicando o tamanho do payload + dados
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

/**
 * @brief Remove o enquadramento do quadro recebido com base na configuração (Receptor).
 */
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

/**
 * @brief Função principal de codificação (Transmissor): valida tamanho, adiciona EDC/FEC e enquadra.
 */
bytes LinkLayer::encode(const bytes &payload) {
  if (payload.size() > config.max_frame_size) throw std::invalid_argument("payload excede o tamanho maximo");
  report("app-tx", payload);

  // Aplica Hamming se configurado, caso contrário aplica o EDC tradicional (Paridade, Checksum ou CRC)
  bytes checked = config.check == CheckType::Hamming ? apply_hamming(payload) : add_check(payload);
  report("enlace-tx", checked);

  bytes frame = apply_framing(checked);
  report("quadro-tx", frame);
  return frame;
}

/**
 * @brief Função principal de decodificação (Receptor): desenquadra, valida erros e entrega o payload.
 */
DecodeResult LinkLayer::decode(const bytes &frame) {
  DecodeResult result;
  try {
    bytes checked = remove_framing(frame);
    report("enlace-rx", checked);

    // Remove Hamming se configurado, caso contrário valida o EDC tradicional
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

/**
 * @brief Processa o fluxo contínuo de dados recebidos em pedaços (streaming / chunks),
 * identificando os limites dos quadros e acionando o callback quando um quadro estiver completo.
 */
void LinkLayer::push(
    const bytes &chunk,
    const std::function<void(const DecodeResult &)> &callback) {

  auto report_frame = [*this](const std::byte value) {
    bytes frame;
    frame.push_back(value);
    frame.insert(frame.begin() + 1, this->stream.begin(), this->stream.end());
    frame.push_back(value);
    report("quadro-rx", frame);
  };

  // Tratamento para protocolos baseados em Flags (Byte Stuffing e Bit Stuffing)
  if (config.framing == FramingType::ByteStuffing ||
      config.framing == FramingType::BitStuffing) {
    for (std::byte value : chunk) {
      uint8_t byte = std::to_integer<uint8_t>(value);
      if (config.framing == FramingType::ByteStuffing) {
        if (byte == FLAG) {
          if (!stream.empty()) {
            report_frame(value);
            callback(decode(stream));
          }
          stream.clear();
          escaping = false;
        } else if (escaping) {
          stream.push_back(std::byte{static_cast<uint8_t>(byte ^ ESC_MASK)});
          escaping = false;
        } else if (byte == ESC)
          escaping = true;
        else
          stream.push_back(value);
      } else {
        if (byte == FLAG) {
          if (!stream.empty()) {
            report_frame(value);
            callback(decode(stream));
          }
          stream.clear();
        } else
          stream.push_back(value);
      }
    }
    return;
  }

  // Tratamento para protocolos baseados em Contagem de Caracteres e
  // Entrelaçamento
  stream.insert(stream.end(), chunk.begin(), chunk.end());
  if ((config.framing == FramingType::CharacterCount ||
       config.framing == FramingType::BitInterleaving) &&
      stream.size() >= 2) {
    size_t size = (std::to_integer<uint8_t>(stream[0]) << 8) |
                  std::to_integer<uint8_t>(stream[1]);
    if (stream.size() >= size + 2) {
      bytes frame(stream.begin(), stream.begin() + size + 2);
      stream.erase(stream.begin(), stream.begin() + size + 2);
      callback(decode(frame));
    }
  }
}

/**
 * @brief Reseta o estado interno do receptor de fluxo (limpa o buffer e estados de escape).
 */
void LinkLayer::reset() {
  stream.clear();
  escaping = false;
}

} // namespace LinkLayer