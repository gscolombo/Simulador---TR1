/**
 * @file transceiver.cpp
 * @brief Implementação da interface entre modem e canal físico.
 * @details
 * O transceptor mantém a conexão Unix com o canal, serializa mensagens por
 * meio do modem para transmissão e acumula os bytes demodulados na fila de
 * recepção.
 */

#include "transceiver.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <mutex>
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
  int fd;
  {
    std::lock_guard<std::mutex> lock(socket_mutex);
    fd = channel_sock;
    channel_sock = -1;
  }

  if (fd < 0)
    return;

  try {
    // Finaliza conexão com canal
    int err = ::shutdown(fd, SHUT_RDWR);
    if (err < 0 && errno != ENOTCONN)
      std::cerr << "Erro ao finalizar conexão com canal" << std::endl;

    // Encerra conexão com canal
    check_err(close(fd),
              "Erro ao fechar socket de entrada de canal.");
  } catch (std::runtime_error &e) {
    std::cout << e.what() << std::endl;
    exit(1);
  }
}

void Transceiver::transmit(const bytes &message) {
  bytes signal;
  modem->toVolts(message, signal);

  int fd;
  {
    std::lock_guard<std::mutex> lock(socket_mutex);
    fd = channel_sock;
  }
  _send(fd, signal);
}

ssize_t Transceiver::receive() {
  bytes data, buf;
  buf.resize(BUF_SIZE);

  int fd;
  {
    std::lock_guard<std::mutex> lock(socket_mutex);
    fd = channel_sock;
  }

  ssize_t N = recv(fd, buf.data(), BUF_SIZE, 0);
  check_err(N, "Erro durante recepção de dados.");
  buf.resize(N);

  modem->fromVolts(buf, data);
  if (data.size() > 0)
    buffer.push_back(data);

  return N;
}
