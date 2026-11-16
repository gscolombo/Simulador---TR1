#include "link_layer.hpp"
#include "modem.hpp"
#include "transceiver.hpp"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace PhysicalLayer;

namespace LL = LinkLayer;
using namespace LinkLayer;

namespace {
std::mutex output_mutex; // Mutex para escrita em stdout

/* Escreve dados de saída da camada de enlace na saída padrão */
void emit_trace(const std::string &stage, const std::string &bits) {
  std::lock_guard lock(output_mutex);
  std::cout << "@LINK|" << stage << "|" << bits << std::endl;
}
} // namespace

void tx(Transceiver *trx, LL::LinkLayer *link, const char *name) {
  std::string message;
  bytes message_bytes;

  auto send_message = [&message_bytes, &trx, &link](std::string m) {
    const std::byte *buf = reinterpret_cast<const std::byte *>(m.data());
    bytes payload(buf, buf + m.size());

    try {
      message_bytes = link->encode(payload);
      trx->transmit(message_bytes);
    } catch (const std::exception &e) {
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

void rx(Transceiver *trx, LL::LinkLayer *link) {
  bytes chunk;

  while (true) {
    if (!trx->receive())
      break;

    if (!trx->buffer.empty()) {
      chunk = trx->buffer.front();
      trx->buffer.pop_front();

      link->push(chunk, [trx](const DecodeResult &result) {
        if (!result.accepted) {
          emit_trace("erro-rx", result.error);
          trx->reset_modem();
          return;
        }

        std::string message(
            reinterpret_cast<const char *>(result.payload.data()),
            result.payload.size());
        std::replace(message.begin(), message.end(), '\n', ' ');
        {
          std::lock_guard lock(output_mutex);
          std::cout << message << std::endl;
        }
        if (result.corrected)
          emit_trace("hamming-corrigido", "1");
        trx->reset_modem();
      });
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
  LL::Config link_config;

  for (int i = 2; i < argc; i++) {
    const std::string arg = argv[i];
    if (arg.rfind("framing=", 0) == 0)
      link_config.framing = framing_from_name(arg.substr(8));
    else if (arg.rfind("check=", 0) == 0)
      link_config.check = check_from_name(arg.substr(6));
    else if (arg.rfind("max_frame=", 0) == 0)
      link_config.max_frame_size = std::stoul(arg.substr(10));
    else
      modem_args.push_back(arg);
  }

  try {
    std::shared_ptr<PhysicalLayer::Modem> modem =
        PhysicalLayer::Config::make_modem(
            PhysicalLayer::Config::parse_args(modem_args));

    Transceiver trx(modem);
    LL::LinkLayer tx_link(link_config), rx_link(link_config);
    tx_link.set_trace_callback(emit_trace);
    rx_link.set_trace_callback(emit_trace);

    trx.connect();

    std::thread tx_t(tx, &trx, &tx_link, name);
    std::thread rx_t(rx, &trx, &rx_link);

    tx_t.join();
    rx_t.join();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
