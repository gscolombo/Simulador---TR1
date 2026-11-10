/**
 * @file types.hpp
 * @brief Tipos compartilhados da camada física.
 */
#ifndef TYPES_H
#define TYPES_H

/** Tamanho máximo de um bloco trocado pelo canal, em bytes. */
#define BUF_SIZE 4096

#include <cstddef>
#include <cstdint>
#include <sys/socket.h>
#include <sys/un.h>
#include <utility>
#include <vector>

namespace PhysicalLayer {

/** Sequência de bytes usada para transmissão de dados e sinais 
    serializados entre processos/threads.
*/
typedef std::vector<std::byte> bytes;
/** Bit lógico; seus valores válidos são 0 e 1. */
typedef uint8_t bit;
/** Bits que representam um símbolo de modulação. */
typedef std::vector<bit> symbol;
/** Amostras elétricas de um símbolo, representadas em ponto flutuante. */
typedef std::vector<double> signal_sample;

/** Coordenada de um ponto na constelação em fase e quadratura (I/Q). */
typedef std::pair<double, double> IQ_coord;
/** Mapeamento entre uma palavra de bits e sua coordenada I/Q. */
typedef std::vector<std::pair<char, IQ_coord>> IQ_map;

/** Endereço Unix usado pelo canal físico local. */
constexpr struct sockaddr_un channel = {AF_UNIX, "/tmp/channel.sock"};
} // namespace PhysicalLayer

#endif