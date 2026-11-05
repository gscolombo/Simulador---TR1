#ifndef TRACE_READER_H
#define TRACE_READER_H

#include "signal_trace.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace PhysicalLayer;

namespace Gui {

// Amostras (em V) da transmissão mais recente de um sinal
struct SignalData {
  std::vector<double> samples;
  std::vector<std::byte> pending; // Bytes de uma amostra ainda incompleta
  uint64_t last_time_ns = 0;
};

/*
  Lê incrementalmente o arquivo de registro de sinal gravado pelo canal. Blocos
  consecutivos de um mesmo transmissor e etapa formam uma transmissão; uma
  pausa maior que BURST_GAP_NS inicia uma nova (descartando a anterior).
*/
class TraceReader {
public:
  static constexpr uint64_t BURST_GAP_NS = 250'000'000;

  explicit TraceReader(std::string path);
  ~TraceReader();
  TraceReader(const TraceReader &) = delete;
  TraceReader &operator=(const TraceReader &) = delete;

  // Lê os registros novos; retorna true se houve dados novos
  bool poll();

  // A referência permanece válida durante toda a vida do leitor
  const SignalData &signal(uint32_t pid, TraceStage stage);

private:
  void process(const TraceHeader &header, const std::byte *payload);

  std::string path;
  int fd = -1;
  std::vector<std::byte> buffer; // Registros lidos e ainda não processados
  std::map<std::pair<uint32_t, TraceStage>, SignalData> signals;
};

} // namespace Gui

#endif
