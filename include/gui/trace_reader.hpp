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

  // Bytes de uma amostra ainda incompleta (o limite de um registro pode cair
  // no meio de uma amostra de 8 bytes)
  std::vector<std::byte> pending;

  // Instante (relógio monotônico) do último registro recebido; serve para
  // detectar a pausa entre duas transmissões
  uint64_t last_time_ns = 0;
};

/*
  Lê incrementalmente o arquivo de registro de sinal gravado pelo canal. Blocos
  consecutivos de um mesmo transmissor e etapa formam uma transmissão; uma
  pausa maior que BURST_GAP_NS inicia uma nova (descartando a anterior).
*/
class TraceReader {
public:
  // Pausa mínima (250 ms) entre dois registros para iniciar uma nova transmissão
  static constexpr uint64_t BURST_GAP_NS = 250'000'000;

  // Apenas guarda o caminho: o arquivo é aberto no primeiro `poll()` em que já
  // existir (o canal o cria ao iniciar)
  explicit TraceReader(std::string path);

  // Fecha o arquivo
  ~TraceReader();

  /**
    @brief Lê o que foi acrescentado ao arquivo e processa os registros
    completos. Um registro incompleto (o canal pode estar escrevendo) fica
    guardado para a próxima chamada. Não bloqueia.
    @return true se algum registro novo foi processado
  */
  bool poll();

  /**
    @brief Obtém as amostras de um transmissor em uma etapa
    @param pid PID do peer transmissor
    @param stage TRACE_IN (transmitido) ou TRACE_OUT (recebido, com ruído)
    @return Dados do sinal; se ainda não houver, uma entrada vazia é criada. A
    referência permanece válida durante toda a vida do leitor, então quem a
    guarda enxerga as amostras novas e a limpeza quando uma transmissão nova
    começa
  */
  const SignalData &signal(uint32_t pid, TraceStage stage);

private:
  /**
    @brief Acrescenta as amostras de um registro ao sinal correspondente.
    Se passou mais de BURST_GAP_NS desde o último registro desse sinal, descarta
    as amostras anteriores (nova transmissão)
    @param header Cabeçalho do registro
    @param payload Bytes de amostras que seguem o cabeçalho (`header.size`)
  */
  void process(const TraceHeader &header, const std::byte *payload);

  std::string path;
  int fd = -1; // -1 enquanto o arquivo não foi aberto

  // Bytes lidos do arquivo e ainda não processados (registro incompleto)
  std::vector<std::byte> buffer;

  // Sinais por (PID do transmissor, etapa)
  std::map<std::pair<uint32_t, TraceStage>, SignalData> signals;
};

} // namespace Gui

#endif