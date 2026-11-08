/**
 * @file channel.hpp
 * @brief Definição da classe Channel.
 * @details Este arquivo contém a definição da classe Channel, que representa o canal de comunicação entre os tranceptores.
 */
#ifndef CHANNEL_H
#define CHANNEL_H

#include "types.hpp"
#include <random>
#include <sys/poll.h>
#include <sys/socket.h>

namespace PhysicalLayer {

#define CHANNEL_DEBUG true

/**
 * @brief Canal físico bidirecional entre dois transceptores.
 * @details
 * O canal representa o meio de comunicação por meio de amostras elétricas de
 * tensão. O modem produz cada amostra como um `double` em volts, e este objeto
 * apenas transporta a representação binária dessas amostras entre os dois
 * transceptores conectados por sockets Unix locais.
 *
 * Antes de encaminhar cada bloco ao destinatário, o canal pode inserir ruído
 * branco gaussiano aditivo (AWGN) diretamente nas amostras de tensão. Para
 * cada amostra `v`, é aplicado `v' = v + n`, em que
 * `n ~ N(noise_mu, noise_sigma^2)`. Assim, `noise_mu` controla a média do
 * ruído e `noise_sigma` controla seu desvio padrão. Quando `noise_sigma` é
 * zero, o sinal é encaminhado sem alteração.
 *
 * A implementação usa sockets apenas como mecanismo de transporte entre as
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
   * @details Aguarda dados em qualquer conexão, interpreta o bloco recebido
   * como amostras `double` de tensão, adiciona o AWGN configurado e envia o
   * bloco ao outro transceptor. O loop termina quando as duas conexões são
   * encerradas.
   */
  void stream();
  /**
   * @brief Fecha o meio e libera seus recursos.
   * @details Fecha o socket de escuta e as conexões dos transceptores e remove
   * o caminho do socket Unix.
   */
  void close();

protected:
private:
  int channel_sock;
  /**
   * @brief Estrutura de socket do canal.
   * @details Esta estrutura representa o socket do canal de comunicação.
   */
  struct pollfd fds[3];
  /**
   * @brief Estrutura de endereço do canal.
   * @details Esta estrutura representa o endereço do socket do canal de comunicação.
   */
  struct sockaddr_un channel;
  /**
   * @brief Média do ruído gaussiano.
   * @details Esta variável armazena a média do ruído gaussiano a ser adicionado ao sinal.
   */
  double noise_mu;
  /**
   * @brief Desvio padrão do ruído gaussiano.
   * @details Esta variável armazena o desvio padrão do ruído gaussiano a ser adicionado ao sinal.
   */
  double noise_sigma;
  std::mt19937 gn;
  std::normal_distribution<double> dist;

  /**
   * @brief Adiciona AWGN diretamente às amostras elétricas.
   * @details Reinterpreta o bloco como uma sequência de valores `double` em
   * volts e soma uma amostra independente de uma distribuição normal com
   * média `noise_mu` e desvio padrão `noise_sigma` a cada valor.
   * @param signal Bloco de amostras de tensão a ser alterado em volts.
   */
  void add_noise(bytes &signal);
};

} // namespace PhysicalLayer

#endif