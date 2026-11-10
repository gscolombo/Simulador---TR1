/**
 * @file transceiver.hpp
 * @brief Interface do transceptor da camada física.
 */
#ifndef TRANSCEIVER_H
#define TRANSCEIVER_H

#include "modem.hpp"
#include "types.hpp"
#include <atomic>
#include <deque>
#include <sys/types.h>
#include <sys/un.h>

namespace PhysicalLayer {

/**
 * @brief Faz a interface entre um modem e o canal físico.
 * @details
 * O transceptor converte dados de aplicação em amostras elétricas por meio do
 * modem, envia essas amostras pelo socket Unix do canal e reconverte os sinais
 * recebidos em bytes. A recepção pode acumular blocos completos na fila
 * pública `buffer`.
 */
class Transceiver {
public:
  /** Indica que o transmissor terminou de preparar seus dados. */
  std::atomic<bool> tx_ready = false, rx_ready = false;
  /** Blocos de dados demodulados aguardando processamento. */
  std::deque<bytes> buffer;

  /**
   * @brief Cria um transceptor associado a um modem.
   * @param modem Modem usado para modular e demodular os sinais.
   */
  Transceiver(std::shared_ptr<Modem> modem);

  /** Conecta o transceptor ao socket do canal físico. */
  void connect();
  /** Encerra a conexão com o canal e libera o socket. */
  void shutdown();
  /** Modula e envia uma mensagem pelo canal físico. */
  void transmit(const bytes &message);
  /** Recebe, demodula e armazena dados vindos do canal. */
  ssize_t receive();

  /** Reinicia o estado interno de recepção do modem. */
  inline void reset_modem() { modem->reset = true; }

  /** Encerra a conexão ao destruir o transceptor. */
  ~Transceiver();

private:
  /** Descritor do socket conectado ao canal. */
  int channel_sock;
  /** Modem responsável pela conversão entre bytes e sinais. */
  std::shared_ptr<Modem> modem;
};

} // namespace PhysicalLayer

#endif