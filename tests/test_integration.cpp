#include "main.hpp"

#include "channel.hpp"
#include "modem.hpp"
#include "transceiver.hpp"
#include "types.hpp"
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

using namespace PhysicalLayer;

void channel_thread(Channel *channel) {

  channel->open();
  std::cout << "CH: Pronto\n";

  channel->stream();
  std::cout << "CH: Mensagem transmitida\n";

  channel->close();
  std::cout << "CH: Saiu\n";
}

void tx_thread(Transceiver *trx, const char *message) {
  // Inicialização de transmissor na camada física
  trx->connect();
  std::cout << "TX: Pronto\n";

  // Preparo de frame (mensagem delimitada por caractere nulo)
  size_t message_length = strlen(message) + 1;
  bytes message_bytes;
  const std::byte *buf = reinterpret_cast<const std::byte *>(message);

  message_bytes.insert(message_bytes.end(), buf, buf + message_length);
  trx->transmit(message_bytes);
  std::cout << "TX: Mensagem enviada\n";

  trx->shutdown();
  std::cout << "TX: Saiu\n";
}

void rx_thread(Transceiver *trx, const char *expected) {
  // Inicialização de receptor na camada física
  trx->connect();
  std::cout << "RX: Pronto\n";

  // Loop de camadas de rede (física -> enlace -> aplicação)
  bytes received, chunk;
  std::string message;

  while (true) {
    // Recebimento de dados na camada física
    if (!trx->receive()) {
      std::cout << "RX: Canal fechou conexão.\n";
      break;
    }

    // Passagem e tratamento para a camada de enlace
    if (!trx->buffer.empty()) {
      chunk = trx->buffer.front();
      trx->buffer.pop_front();

      received.insert(received.end(), chunk.begin(), chunk.end());

      std::cout << "RX: Dados recebidos (" << received.size() << " de "
                << strlen(expected) + 1 << " bytes)\n";

      // Verificação de delimitador
      if (!received.empty() && received.back() == std::byte{0}) {
        message = reinterpret_cast<char *>(received.data());
        CHECK(message == expected);
        trx->reset_modem();
        break;
      }
    }
  }

  trx->shutdown();
  std::cout << "RX: Saiu" << std::endl;
}

TEST_CASE("Transmissão e recepção de mensagem em canal perfeito") {
  std::vector<std::pair<const char *, std::shared_ptr<Modem>>> modems;
  Channel channel(0., 0.);

  const char *messages[3]{
      "Hello World!",
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
      "eiusmod "
      "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
      "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex "
      "ea "
      "commodo consequat. Duis aute irure dolor in reprehenderit in "
      "voluptate "
      "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
      "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
      "mollit anim id est laborum.",
      "From this distant vantage point [that of ‘an alien scientist newly "
      "arrived at the\n"
      "outskirts of our solar system’ where Voyager 1 took the photograph], "
      "the Earth\n"
      "might not seem of any particular interest.\n"
      "But for us, it’s different. Look again at that dot. That’s here. "
      "That’s home. That’s\n"
      "us. On it everyone you love, everyone you know, everyone you ever "
      "heard of, every\n"
      "human being who ever was, lived out their lives. The aggregate of our "
      "joy and\n"
      "suffering, thousands of confident religions, ideologies, and economic "
      "doctrines,\n"
      "every hunter and forager, every hero and coward, every creator and "
      "destroyer of\n"
      "civilization, every king and peasant, every young couple in love, "
      "every mother and\n"
      "father, hopeful child, inventor and explorer, every teacher of "
      "morals, every\n"
      "corrupt politician, every ‘superstar,’ every ‘supreme leader,’ every "
      "saint and\n"
      "sinner in the history of our species lived there — on a mote of dust "
      "suspended in a\n"
      "sunbeam.\n"
      "The Earth is a very small stage in a vast cosmic arena. Think of the "
      "rivers of\n"
      "blood spilled by all those generals and emperors so that, in glory "
      "and triumph,\n"
      "they could become the momentary masters of a fraction of a dot. Think "
      "of the\n"
      "endless cruelties visited by the inhabitants of one corner of this "
      "pixel on the\n"
      "scarcely distinguishable inhabitants of some other corner, how "
      "frequent their\n"
      "misunderstandings, how eager they are to kill one another, how "
      "fervent their\n"
      "hatreds.\n"
      "Our posturings, our imagined self-importance, the delusion that we "
      "have some\n"
      "privileged position in the Universe, are challenged by this point of "
      "pale light. Our\n"
      "planet is a lonely speck in the great enveloping cosmic dark. In our "
      "obscurity, in\n"
      "all this vastness, there is no hint that help will come from "
      "elsewhere to save us\n"
      "from ourselves.\n"
      "The Earth is the only world known so far to harbor life. There is "
      "nowhere else, at\n"
      "least in the near future, to which our species could migrate. Visit, "
      "yes. Settle, not\n"
      "yet. Like it or not, for the moment the Earth is where we make our "
      "stand.\n"
      "It has been said that astronomy is a humbling and character-building "
      "experience.\n"
      "There is perhaps no better demonstration of the folly of human "
      "conceits than this\n"
      "distant image of our tiny world. To me, it underscores our "
      "responsibility to deal\n"
      "more kindly with one another, and to preserve and cherish the pale "
      "blue dot, the\n"
      "only home we’ve ever known."};

  auto test = [&modems, &channel, &messages]() {
    for (auto modem : modems) {
      std::cout << std::endl << "Modem: " << modem.first;

      for (auto message : messages) {
        std::cout << std::endl << "Mensagem: " << message << std::endl;

        Transceiver tx(modem.second->clone());
        Transceiver rx(modem.second->clone());

        std::thread tx_t(tx_thread, &tx, message);
        std::thread rx_t(rx_thread, &rx, message);
        std::thread channel_t(channel_thread, &channel);

        tx_t.join();
        rx_t.join();
        channel_t.join();
      }
    }
  };

  SUBCASE("Modems banda base") {
    modems.push_back(
        {"NRZ Polar", std::make_shared<NRZPolarModem>(1, 100, 1e3, 2.)});
    modems.push_back(
        {"Manchester", std::make_shared<ManchesterModem>(1, 100, 1e3, 2.)});
    modems.push_back(
        {"Bipolar", std::make_shared<BipolarModem>(1, 100, 1e3, 2.)});

    test();

    modems.clear();
  }

  SUBCASE("Modems banda passante") {
    modems.push_back(
        {"ASK", std::make_shared<ASKModem>(1, 4e3, 100e3, 500, 2.)});
    modems.push_back(
        {"FSK", std::make_shared<FSKModem>(1, 4e3, 100e3, 500, 2., 10e3)});
    modems.push_back(
        {"8PSK", std::make_shared<_8PSKModem>(3, 4e3, 100e3, 500, 2.)});

    test();

    modems.clear();
  }
}
