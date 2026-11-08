/**
 * @file channel.cpp
 * @brief Implementação do canal físico com ruído AWGN.
 * @details
 * O canal recebe do modem blocos binários que contêm amostras `double` de
 * tensão. O socket Unix é somente o suporte de transporte entre os
 * transceptores: antes do encaminhamento, cada amostra é convertida
 * conceitualmente para `double`, recebe ruído gaussiano aditivo e volta a ser
 * enviada como bytes.
 *
 * Para uma amostra de tensão `v`, a degradação simulada é:
 * `v' = v + n`, com `n ~ N(noise_mu, noise_sigma^2)`. Logo, os parâmetros do
 * construtor permitem variar a interferência média e sua dispersão, enquanto
 * `noise_sigma == 0` mantém o canal ideal.
 */

#include "channel.hpp"
#include "types.hpp"

#include "utils.hpp"
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace PhysicalLayer;

Channel::Channel(double noise_mu, double noise_sigma)
    : noise_mu(noise_mu), noise_sigma(noise_sigma) {
  if (noise_sigma == 0.)
    return;

  // Configura gerador gaussiano de números pseudo-aleatórios
  std::random_device seed;

  gn = std::mt19937(seed());
  dist = std::normal_distribution<double>(noise_mu, noise_sigma);
}

void Channel::open() {
  if (CHANNEL_DEBUG)
    std::filesystem::create_directory("/tmp/channel");

  unlink(channel.sun_path);

  const char *errMessage = "Erro durante inicialização de canal.";

  // Inicializa objeto de socket do canal
  channel_sock = socket(AF_UNIX, SOCK_STREAM, 0);
  check_err(channel_sock, "Erro durante criação de objeto de socket de canal.");

  // Prepara canal para receber conexões
  check_err(bind(channel_sock, (struct sockaddr *)&channel, sizeof(channel)),
            errMessage);
  check_err(listen(channel_sock, 1), errMessage);

  // Inicializa o descritor do canal
  fds[0].fd = channel_sock;
  fds[0].events = POLLIN;

  // Inicializa os descritores dos tranceptores
  fds[1].fd = -1;
  fds[2].fd = -1;

  // Aguarda e bloqueia até o primeiro transceptor conectar
  fds[1].fd = accept(channel_sock, nullptr, nullptr);
  check_err(fds[1].fd, "Erro durante aceite da primeira conexão.");
  fds[1].events = POLLIN;

  // Aguarda e bloqueia até o segundo transceptor conectar
  fds[2].fd = accept(channel_sock, nullptr, nullptr);
  check_err(fds[2].fd, "Erro durante aceite da segunda conexão.");
  fds[2].events = POLLIN;
}

void Channel::stream() {
  bytes signal(BUF_SIZE);

  try {
    while (true) {
      // Aguarda por algum transceptor estar pronto para enviar dados
      int ev = poll(fds, 3, 100);
      check_err(ev, "Erro durante polling no canal.");

      // Os dois tranceptores fecharam a conexão
      if (fds[1].fd == -1 && fds[2].fd == -1)
        break;

      if (ev > 0)
        for (int i : {1, 2}) {
          // Verifica se algum transceptor enviou dados
          if (fds[i].fd > 0 && (fds[i].revents & POLLIN)) {
            int n = recv(fds[i].fd, signal.data(), BUF_SIZE, 0);
            check_err(n, "Erro durante recebimento de dados no canal.");

            // Transmissor fechou a conexão
            if (!n) {
              fds[i].fd = -1;
              continue;
            }

            signal.resize(n);

            if (CHANNEL_DEBUG) {
              std::ofstream in;
              std::ios::openmode mode =
                  std::ios::binary | std::ios::out | std::ios::app;

              in.open("/tmp/channel/in", mode);
              in.write((const char *)signal.data(), signal.size());
              in.close();
            }

            // Determina qual o destinatário dos dados
            int recipient = i == 1 ? fds[2].fd : fds[1].fd;
            if (recipient < 0) {
              std::cout << "Dados perdidos." << std::endl;
              break;
            }

            add_noise(signal);

            if (CHANNEL_DEBUG) {
              std::ofstream out;
              std::ios::openmode mode =
                  std::ios::binary | std::ios::out | std::ios::app;

              out.open("/tmp/channel/out", mode);
              out.write((const char *)signal.data(), signal.size());
              out.close();
            }

            int m = send(recipient, signal.data(), n, 0);
            check_err(m, "Erro durante transmissão de dados pelo canal.");

            signal.resize(BUF_SIZE);
          }
        }
    }
  } catch (const std::runtime_error &e) {
    std::cout << e.what() << std::endl;
  }
}

void Channel::close() {
  for (auto fd : fds)
    ::close(fd.fd);

  unlink(channel.sun_path);
}

void Channel::add_noise(bytes &signal) {
  if (noise_sigma == 0.)
    return;

  const size_t bufsize = signal.size();
  assert(bufsize % sizeof(double) == 0);

  // Interpreta buffer de bytes como ponto-flutuante (8 bytes)
  double *data = reinterpret_cast<double *>(signal.data());

  // Adiciona ruído gaussiano ao sinal
  for (double *D = data; D < data + bufsize / sizeof(double); D++)
    *D += dist(gn);
}

Channel::~Channel() { close(); }