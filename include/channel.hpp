/**
 * @file channel.hpp
 * @brief Definição da classe Channel.
 * @details Este arquivo contém a definição da classe Channel, que representa o
 * canal de comunicação entre os tranceptores.
 */
#ifndef CHANNEL_H
#define CHANNEL_H

#define INIT_POLLFD {-1, POLLIN, 0}

#include "gui/signal_trace.hpp"
#include "types.hpp"
#include <random>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>

namespace PhysicalLayer {

/**
 * @brief Canal físico bidirecional entre dois transceptores.
 * @details
 * O canal representa o meio de comunicação por meio de amostras de sinais
 * elétricos de tensão. O modem produz cada amostra como um `double` em volts, e
 * este objeto apenas transporta a representação binária dessas amostras entre
 * os dois transceptores conectados por sockets Unix locais.
 *
 * Antes de encaminhar cada bloco ao destinatário, o canal pode inserir ruído
 * branco gaussiano aditivo (AWGN) diretamente nas amostras de tensão. Para
 * cada amostra `v`, é aplicado `v' = v + n`, em que
 * `n ~ N(noise_mu, noise_sigma^2)`. Assim, `noise_mu` controla a média do
 * ruído (componente DC) e `noise_sigma` controla seu desvio padrão.
 * Quando `noise_sigma` é zero, o sinal é encaminhado sem adição de ruído.
 *
 * A implementação usa sockets apenas como mecanismo de comunicação entre as
 * threads/processos; eles não substituem a grandeza física do sinal, que é a
 * sequência de valores de tensão gerada pelo modem.
 */
class Channel {
public:
  /**
   * @brief Cria um canal com os parâmetros do ruído AWGN.
   * @param noiseMu Média, em volts, da variável aleatória do ruído.
   * @param noiseSigma Desvio padrão, em volts, da variável aleatória do ruído.
   */
  Channel(double noiseMu, double noiseSigma);
  /**
   * @brief Destrutor da classe Channel.
   * @details Este método fecha o canal de comunicação ao destruir o objeto.
   */
  ~Channel();
  /**
   * @brief Abre o meio e conecta os dois transceptores.
   * @details Cria o socket Unix local, aguarda exatamente duas conexões e
   * prepara os descritores usados pelo loop de transmissão.
   */
  void open();
  /**
   * @brief Encaminha sinais entre os transceptores.
   * @details Aguarda dados em qualquer conexão, acumula bytes até obter pelo
   * menos uma amostra (`double`) de sinal de tensão, adiciona o AWGN
   * configurado e encaminha o sinal resultante ao outro transceptor. Retorna
   * quando as duas conexões são encerradas.
   */
  void stream();
  /**
   * @brief Fecha o meio e libera seus recursos.
   * @details Fecha o socket de escuta e as conexões dos transceptores e remove
   * o caminho do socket Unix.
   */
  void close();

  // Habilita a gravação do sinal (antes e depois do ruído) em arquivo, no
  // formato descrito em signal_trace.hpp.
  void set_trace_file(const std::string &path) { trace_path = path; }

protected:
private:
  /** Descritor do socket de escuta do canal. */
  int channel_sock;
  /**
   * @brief Estruturas de descritores de socket gerenciados pelo canal com
   * suporte para polling
   * @details Cada estrutura contém o descritor do arquivo, os eventos
   * monitorados e os eventos retornados. No caso, somente o evento POLLIN
   * (quando dados estão disponíveis para leitura) é monitorado.
   */
  struct pollfd fds[3] = {INIT_POLLFD, INIT_POLLFD, INIT_POLLFD};
  /**
   * @brief Média do ruído gaussiano.
   * @details Esta variável armazena a média do ruído gaussiano a ser adicionado
   * ao sinal.
   */
  double noise_mu;
  /**
   * @brief Desvio padrão do ruído gaussiano.
   * @details Esta variável armazena o desvio padrão do ruído gaussiano a ser
   * adicionado ao sinal.
   */
  double noise_sigma;
  /** Gerador pseudoaleatório usado para produzir o ruído. */
  std::mt19937 gn;
  /** Distribuição normal configurada com a média e o desvio do ruído. */
  std::normal_distribution<double> dist;

  /* Caminho para arquivo de visualização de sinal */
  std::string trace_path;
  int trace_fd = -1; // Descritor do arquivo de visualização de sinal
  uint32_t peer_pid[3] = {0, 0, 0}; // PID do processo conectado em cada fds[i]

  /**
   * @brief Adiciona AWGN diretamente às amostras elétricas.
   * @details Reinterpreta o bloco como uma sequência de valores `double` em
   * volts e soma uma amostra independente de uma distribuição normal com
   * média `noise_mu` e desvio padrão `noise_sigma` a cada valor.
   * @param signal Bloco de amostras de tensão a ser alterado em volts.
   */
  void add_noise(bytes &signal);
  /**
   * @brief Registra uma amostra do sinal elétrico recebido/transmitido pelo
   * canal.
   * @details O arquivo para visualização do sinal é formado com registros de
   * amostras com um cabeçalho contendo o PID do processo remetente, o tamanho
   * da amostra em bytes, o tempo em que foi processada e o estado do sinal
   * (antes ou após adição de ruído).
   * @param stage Estágio do sinal durante passagem pelo canal (antes ou após
   * adição de ruído)
   * @param sender PID do processo remetente
   * @param signal Amostra de sinal elétrico em bytes
   */
  void trace(TraceStage stage, int sender, const bytes &signal);
};

} // namespace PhysicalLayer

#endif