#ifndef LINK_LAYER_H
#define LINK_LAYER_H

#include "types.hpp"
#include <cstddef>
#include <functional>
#include <string>

namespace LinkLayer {

/**
 * @brief Define os tipos de enquadramento suportados pela camada de enlace.
 */
enum class FramingType { 
    CharacterCount,  // Contagem de caracteres no cabeçalho
    BitInterleaving, // Entrelaçamento de bits
    ByteStuffing,    // Enquadramento com flags e escape de bytes (PPP)
    BitStuffing      // Enquadramento com flags e inserção de bits (HDLC)
};

/**
 * @brief Define os métodos de detecção (EDC) e correção (FEC) de erros.
 */
enum class CheckType { 
    None,       // Sem verificação
    EvenParity, // Bit de paridade par
    Checksum,   // Soma de verificação (Checksum)
    CRC32,      // Cyclic Redundancy Check de 32 bits (IEEE 802)
    Hamming     // Código Corretor de Hamming (FEC)
};

/**
 * @brief Estrutura de configuração da camada de enlace.
 */
struct Config {
    FramingType framing = FramingType::ByteStuffing; // Protocolo de enquadramento padrão
    CheckType check = CheckType::CRC32;              // Método de verificação padrão
    size_t max_frame_size = 256;                     // Tamanho máximo permitido para o quadro
};

/**
 * @brief Representa o resultado retornado pelo receptor após processar e decodificar um quadro.
 */
struct DecodeResult {
    bool accepted = false;             // Indica se o quadro passou na verificação e foi aceito
    bool corrected = false;            // Indica se houve correção de erro (utilizado pelo Hamming)
    std::string error;                 // Mensagem descritiva caso ocorra falha ou rejeição
    PhysicalLayer::bytes payload;      // Carga útil (dados úteis) recuperada e limpa
};

// Atalho para a função de callback de rastreamento/telemetria (estágio e representação binária)
using TraceCallback = std::function<void(const std::string &, const std::string &)>;

// Funções utilitárias para conversão entre enums e representações em formato de texto
const char *framing_name(FramingType type);
const char *check_name(CheckType type);
FramingType framing_from_name(const std::string &name);
CheckType check_from_name(const std::string &name);

/**
 * @brief Classe principal responsável pela codificação e decodificação de quadros na camada de enlace.
 */
class LinkLayer {
public:
    // Construtor explícito que inicializa a camada com uma configuração opcional
    explicit LinkLayer(Config config = {});

    // Registra a função de callback para telemetria/rastreamento dos quadros
    void set_trace_callback(TraceCallback callback);
    
    // Transmissor: adiciona redundância, enquadra e prepara o payload para envio
    PhysicalLayer::bytes encode(const PhysicalLayer::bytes &payload);
    
    // Receptor: desenquadra, valida a integridade e extrai o payload do quadro bruto
    DecodeResult decode(const PhysicalLayer::bytes &frame);

    // Receptor por fluxo (streaming): consome pedaços de dados (chunks) e dispara o callback por quadro completo
    void push(const PhysicalLayer::bytes &chunk,
              const std::function<void(const DecodeResult &)> &callback);
              
    // Reseta o estado interno do receptor de fluxo (limpa buffers e flags de escape)
    void reset();

private:
    // Atributos de estado e configuração
    Config config;                // Configurações ativas da camada de enlace
    TraceCallback trace;          // Função de rastreamento registrada
    PhysicalLayer::bytes stream;  // Buffer acumulador temporário para recepção de fluxo contínuo
    bool escaping = false;        // Bandeira de controle de escape para Byte Stuffing

    // Métodos auxiliares privados (lógica interna de enquadramento, EDC e FEC)
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