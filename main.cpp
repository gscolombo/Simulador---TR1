#include "channel.hpp"
#include "modem.hpp"
#include "transceiver.hpp"
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

using namespace std;
using namespace PhysicalLayer;

atomic<bool> sending = false;

void run_channel(Channel *channel) {
  channel->open();
  cout << "CH: Pares conectados\n";
  channel->stream();
  channel->close();
  cout << "CH: Saiu\n";
}

void run_tx(Transceiver *trx) {
  trx->connect();
  cout << "TX: Pronto\n";

  string message;
  bytes message_bytes;

  while (true) {
    if (sending.load())
      sending.wait(true);

    cout << "TX >> ";
    getline(cin, message);

    const byte *buf = reinterpret_cast<const byte *>(message.data());
    message_bytes.insert(message_bytes.end(), buf, buf + message.size());
    message_bytes.push_back(byte{0});

    sending.store(true);
    trx->transmit(message_bytes);
    message_bytes.clear();

    if (message == ":q")
      break;
  }

  trx->shutdown();
  cout << "TX: Saiu\n";
}

void run_rx(Transceiver *trx) {
  // Inicializa receptor e conexão com canal
  trx->connect();
  cout << "RX: Pronto\n";

  // Loop de camadas de rede (física -> enlace -> aplicação)
  bytes received, chunk;
  string message;

  while (true) {
    // Recebimento de dados na camada física
    if (!trx->receive()) {
      cout << "RX: Canal fechou conexão.\n";
      break;
    }

    // Passagem e tratamento para a camada de enlace
    if (!trx->buffer.empty()) {
      chunk = trx->buffer.front();
      trx->buffer.pop_front();

      received.insert(received.end(), chunk.begin(), chunk.end());

      // Verificação de delimitador
      if (!received.empty() && received.back() == byte{0}) {
        message = reinterpret_cast<char *>(received.data());
        if (message == ":q")
          break;

        cout << message << " << RX\n";
        received.clear();
        trx->reset_modem();
        sending.store(false);
        sending.notify_one();
      }
    }
  }

  trx->shutdown();
  std::cout << "RX: Saiu" << std::endl;
}

int main() {
  shared_ptr<Modem> modem = make_shared<_8PSKModem>(3, 4000, 100e3, 10e3, 1.);

  Channel channel(0., 5e-2);
  Transceiver alice(modem);
  Transceiver bob(modem);

  thread ch_thread(&run_channel, &channel);
  thread tx_thread(&run_tx, &alice);
  thread rx_thread(&run_rx, &bob);

  tx_thread.join();
  ch_thread.join();
  rx_thread.join();
}