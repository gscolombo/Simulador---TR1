#include "config/modem.hpp"
#include "modem.hpp"
#include "transceiver.hpp"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace PhysicalLayer;

void tx(Transceiver *trx, const char *name) {
  std::string message;
  bytes message_bytes;

  auto send_message = [&message_bytes, &trx](std::string m) {
    const std::byte *buf = reinterpret_cast<const std::byte *>(m.data());
    message_bytes.insert(message_bytes.end(), buf, buf + m.size());
    message_bytes.push_back(std::byte{0});

    try {
      trx->transmit(message_bytes);
    } catch (std::runtime_error &e) {
      std::cerr << e.what() << std::endl;
    }
    message_bytes.clear();
  };

  std::string salute = std::string(name) + " entrou";
  send_message(salute);

  while (true) {
    // Fim da entrada (EOF) equivale a sair do chat
    if (!getline(std::cin, message))
      message = ":q";

    if (message == ":q") {
      std::string farewell = std::string(name) + " saiu";
      send_message(farewell);
      break;
    }

    send_message(message);
  }

  trx->shutdown();
}

void rx(Transceiver *trx) {
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

      if (received.size() > 255)
        received.clear();

      // Verificação de delimitador
      if (!received.empty() && received.back() == std::byte{0}) {
        message = reinterpret_cast<char *>(received.data());

        // Remove quebra de linha ao final da mensagem
        std::replace(message.begin(), message.end(), '\n', ' ');
        std::cout << message << std::endl;

        received.clear();
        trx->reset_modem();
      }
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Passe o nome do usuário" << std::endl;
    return -1;
  }

  const char *name = argv[1];
  std::vector<std::string> modem_args;

  for (int i = 2; i < argc; i++)
    modem_args.push_back(argv[i]);

  try {
    std::shared_ptr<PhysicalLayer::Modem> modem =
        Config::Modem::make_modem(Config::Modem::parse_args(modem_args));

    Transceiver trx(modem);

    trx.connect();

    std::thread tx_t(tx, &trx, name);
    std::thread rx_t(rx, &trx);

    tx_t.join();
    rx_t.join();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
