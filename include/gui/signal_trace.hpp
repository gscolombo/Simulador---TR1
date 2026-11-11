#ifndef SIGNAL_TRACE_H
#define SIGNAL_TRACE_H

#include <cstdint>

namespace PhysicalLayer {

/*
  Formato do arquivo de visualização de sinal gravado pelo canal e lido
  pela GUI. O arquivo é uma sequência de registros, cada um com
  um cabeçalho seguido de `size` bytes de amostras (double, em V).
*/

/*
  Enumeração do estado do sinal durante a passagem pelo canal.
*/
enum TraceStage : uint8_t {
  TRACE_IN = 0, // Sinal transmitido (entrada do canal, antes do ruído)
  TRACE_OUT = 1 // Sinal recebido (saída do canal, com ruído)
};

/*
  Cabeçalho dos registros do arquivo de visualização de sinal.
*/
struct TraceHeader {
  uint32_t source_pid; // Processo transmissor do sinal
  uint32_t size;       // Bytes de amostras que seguem o cabeçalho
  uint64_t time_ns;    // Instante do registro (relógio monotônico)
  TraceStage stage;    // TraceStage
  uint8_t reserved[7];
};

static_assert(sizeof(TraceHeader) == 24,
              "Cabeçalho de registro deve ter 24 bytes");

} // namespace PhysicalLayer

#endif
