#include "modem.hpp"
#include "transceiver.hpp"
#include "include/link.hpp"
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace PhysicalLayer;
using namespace DataLinkLayer;

void tx(Transceiver *trx, LinkLayer *link, const char *name) {
  std::string message;
  bytes message_bytes;

  auto send_message = [&message_bytes, &trx, &link](std::string m) {
    const std::byte *buf = reinterpret_cast<const std::byte *>(m.data());
    message_bytes.insert(message_bytes.end(), buf, buf + m.size());
    message_bytes.push_back(std::byte{0});

    try {
      bytes framed_message = link->apply_framing(message_bytes);
      std::cout << "Quadro HEX (TX): "; //teste
      for (auto b : framed_message) {
        printf("%02X ", std::to_integer<int>(b));
      }
      std::cout << "\nTX >> " << std::flush; //teste
      trx->transmit(framed_message);
    } catch (std::runtime_error &e) {
      std::cerr << e.what() << std::endl;
    }
    message_bytes.clear();
  };

  std::string salute = std::string(name) + " entrou";
  send_message(salute);

  while (true) {
    std::cout << "TX >> " << std::flush;
    getline(std::cin, message);

    if (message == ":q") {
      std::string farewell = std::string(name) + " saiu";
      send_message(farewell);
      break;
    }

    send_message(message);
  }

  trx->shutdown();
}

void rx(Transceiver *trx, LinkLayer *link) {
  // Loop de camadas de rede (física -> enlace -> aplicação)
  bytes received, chunk;
  std::string message;

  while (true) {
    if (!trx->receive())
      break;

    // Passagem e tratamento para a camada de enlace
    if (!trx->buffer.empty()) {
      chunk = trx->buffer.front();
      trx->buffer.pop_front();

      received.insert(received.end(), chunk.begin(), chunk.end());

      if (received.size() > 512)
        received.clear();
      
      // 1. CAMADA DE ENLACE: Tenta extrair o payload do quadro recebido
      bool ok = false;
      bytes payload = link->remove_framing(received, ok);

      // 2. APLICAÇÃO: Se 'ok' for true, o quadro está completo e processado
      if (ok && !payload.empty() && payload.back() == std::byte{0}) {
        message = reinterpret_cast<char *>(payload.data());

        std::cout << "\033[2K\r" << message << " << RX\nTX >> " << std::flush;
        received.clear();
        trx->reset_modem();
      }
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "Passe o nome do usuário" << std::endl;
    return -1;
  }

  const char *name = argv[1];

  std::shared_ptr<_32QAM> modem = std::make_shared<_32QAM>(2e3, 10e3, 1e3, 2.);

  Transceiver trx(modem);

  LinkLayer link(BYTE_STUFFING);

  trx.connect();

  std::thread tx_t(tx, &trx, &link, name);
  std::thread rx_t(rx, &trx, &link);

  tx_t.join();
  rx_t.join();

  return 0;
}