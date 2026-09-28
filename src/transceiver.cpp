#include "transceiver.hpp"
#include "channel.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <utility>

using namespace PhysicalLayer;

Transceiver::Transceiver(std::shared_ptr<Modem> modem)
    : modem(std::move(modem)) {
  try {
    // Inicializa socket para transmissão no canal
    channel_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    check_err(channel_sock, "Erro na criação de socket de entrada de canal.");
  } catch (std::runtime_error &e) {
    std::cout << e.what() << std::endl;
    exit(1);
  }
};

Transceiver::~Transceiver() { shutdown(); }

void Transceiver::connect() {
  while (true) {
    // Espera para arquivo de socket ser criado pelo canal
    if (!std::filesystem::exists(channel.sun_path))
      std::this_thread::sleep_for(std::chrono::milliseconds{100});

    int err =
        ::connect(channel_sock, (struct sockaddr *)&channel, sizeof(channel));
    if (err < 0) {
      if (errno != ECONNREFUSED && errno != ENOENT && errno != EINTR)
        throw std::runtime_error("Erro ao conectar transceptor com canal.");
      close(channel_sock);
      channel_sock = socket(AF_UNIX, SOCK_STREAM, 0);
      std::this_thread::sleep_for(std::chrono::milliseconds{500});
    } else
      break;
  }
}

void Transceiver::shutdown() {
  if (channel_sock < 0)
    return;

  try {
    // Finaliza conexão com canal
    int err = ::shutdown(channel_sock, SHUT_RDWR);
    if (err < 0 && errno != ENOTCONN)
      std::cerr << "Erro ao finalizar conexão com canal" << std::endl;

    // Encerra conexão com canal
    check_err(close(channel_sock),
              "Erro ao fechar socket de entrada de canal.");

    channel_sock = -1;
  } catch (std::runtime_error &e) {
    std::cout << e.what() << std::endl;
    exit(1);
  }
}

void Transceiver::transmit(const bytes &message) {
  if (CHANNEL_DEBUG) {
    if (std::filesystem::exists("/tmp/channel/in"))
      truncate("/tmp/channel/in", 0);

    if (std::filesystem::exists("/tmp/channel/out"))
      truncate("/tmp/channel/out", 0);
  }

  bytes signal;
  modem->toVolts(message, signal);

  check_err(send(channel_sock, signal.data(), signal.size(), 0),
            "Erro durante envio de mensagem.");
}

ssize_t Transceiver::receive() {
  bytes data, buf;
  buf.resize(BUF_SIZE);

  ssize_t N = recv(channel_sock, buf.data(), BUF_SIZE, 0);
  check_err(N, "Erro durante recepção de dados.");
  buf.resize(N);

  modem->fromVolts(buf, data);
  if (data.size() > 0)
    buffer.push_back(data);

  return N;
}
