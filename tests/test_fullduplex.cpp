#include "channel.hpp"
#include "main.hpp"
#include "modem.hpp"
#include "transceiver.hpp"

#include <string>
#include <thread>

using namespace PhysicalLayer;

void channel_thread(Channel *channel) {

  channel->open();
  std::cout << "CH: Pronto\n";

  channel->stream();
  std::cout << "CH: Mensagem transmitida\n";

  channel->close();
  std::cout << "CH: Saiu\n";
}

TEST_CASE("Fluxo bidirecional e simultâneo de dados (full-duplex)") {
  // Utiliza um modem simples em banda base para acelerar a execução do teste
  Channel channel(0., 0.);

  const char *msg_alice_to_bob = "Hey Bob.";
  const char *msg_bob_to_alice = "Hi Alice!";

  // Lambda que simula um nó completo operando TX e RX concorrentemente
  auto transceiver_node = [](std::string peer, Transceiver *trx,
                             const char *msg_out, const char *expected_in) {
    trx->connect();
    std::cout << peer + std::string(" conectado\n");

    // Tarefa de transmissão
    auto tx_task = [&trx, &msg_out]() {
      size_t len = strlen(msg_out) + 1;
      bytes out_bytes;
      const std::byte *buf = reinterpret_cast<const std::byte *>(msg_out);

      out_bytes.insert(out_bytes.end(), buf, buf + len);
      trx->transmit(out_bytes);
    };

    // Tarefa de recepção
    auto rx_task = [&trx, &expected_in]() {
      bytes received, chunk;

      while (true) {
        if (!trx->receive())
          break;

        while (!trx->buffer.empty()) {
          chunk = trx->buffer.front();
          trx->buffer.pop_front();

          received.insert(received.end(), chunk.begin(), chunk.end());

          if (!received.empty() && received.back() == std::byte{0}) {
            std::string msg = reinterpret_cast<char *>(received.data());
            CHECK(msg == expected_in);
            return; // Encerra a tarefa ao validar a mensagem
          }
        }
      }
    };

    // Inicia fluxo full-duplex
    std::thread t_rx(rx_task);
    std::thread t_tx(tx_task);

    t_tx.join();
    t_rx.join();
  };

  // Instancia os transceptores para este ciclo
  NRZPolarModem modem(100, 1e3, 2.);

  Transceiver alice(modem.clone());
  Transceiver bob(modem.clone());

  std::thread channel_t(channel_thread, &channel);
  std::thread alice_t(transceiver_node, "Alice", &alice, msg_alice_to_bob,
                      msg_bob_to_alice);
  std::thread bob_t(transceiver_node, "Bob", &bob, msg_bob_to_alice,
                    msg_alice_to_bob);

  alice_t.join();
  bob_t.join();

  alice.shutdown();
  bob.shutdown();

  channel_t.join();
}