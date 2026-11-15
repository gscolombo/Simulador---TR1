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
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <random>
#include <stdexcept>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

using namespace PhysicalLayer;

namespace {
// Retorna o PID do processo que se conectou ao socket (0 se não for possível
// obter)
uint32_t connected_pid(int fd) {
  struct ucred cred = {};
  socklen_t len = sizeof(cred);

  if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0)
    return 0;

  return cred.pid;
}

} // namespace

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
  unlink(channel.sun_path);

  const char *errMessage = "Erro durante inicialização de canal.";

  // Cria (ou zera) o arquivo de visualização do sinal antes dos transceptores
  // conectarem
  if (!trace_path.empty()) {
    trace_fd =
        ::open(trace_path.c_str(),
               O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_CLOEXEC, 0644);
    check_err(trace_fd, "Erro ao criar arquivo de registro do sinal.");
  }

  // Inicializa objeto de socket do canal
  channel_sock = socket(AF_UNIX, SOCK_STREAM, 0);
  check_err(channel_sock, "Erro durante criação de objeto de socket de canal.");

  // Prepara canal para receber conexões
  check_err(bind(channel_sock, (struct sockaddr *)&channel, sizeof(channel)),
            errMessage);
  check_err(listen(channel_sock, 2), errMessage);

  // Seta o descritor do socket do canal
  fds[0].fd = channel_sock;

  // Aguarda e bloqueia até o primeiro transceptor conectar
  fds[1].fd = accept(channel_sock, nullptr, nullptr);
  check_err(fds[1].fd, "Erro durante aceite da primeira conexão.");
  peer_pid[1] = connected_pid(fds[1].fd);

  // Aguarda e bloqueia até o segundo transceptor conectar
  fds[2].fd = accept(channel_sock, nullptr, nullptr);
  check_err(fds[2].fd, "Erro durante aceite da segunda conexão.");
  peer_pid[2] = connected_pid(fds[2].fd);
}

void Channel::stream() {
  bytes chunk(BUF_SIZE);
  bytes pending[3];

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
          // Verifica se o i-ésimo transceptor enviou dados
          if (fds[i].fd > 0 && (fds[i].revents & POLLIN)) {
            int n = recv(fds[i].fd, chunk.data(), BUF_SIZE, 0);
            check_err(n, "Erro durante recebimento de dados no canal.");

            // Transmissor fechou a conexão
            if (!n) {
              fds[i].fd = -1;
              continue;
            }

            // Verifica se ao menos um `double` foi recebido
            pending[i].insert(pending[i].end(), chunk.begin(), chunk.begin() + n);
            size_t ready = (pending[i].size() / sizeof(double)) * sizeof(double);
            if (ready == 0)
              continue;

            bytes signal(pending[i].begin(), pending[i].begin() + ready);
            pending[i].erase(pending[i].begin(), pending[i].begin() + ready);

            trace(TRACE_IN, i, signal); // Registro de sinal sem ruído

            // Determina qual o destinatário dos dados
            int recipient = i == 1 ? fds[2].fd : fds[1].fd;
            if (recipient < 0) {
              std::cout << "Dados perdidos." << std::endl;
              break;
            }

            // Adiciona ruído, se aplicável
            if (noise_sigma > 0.)
              add_noise(signal);

            trace(TRACE_OUT, i, signal); // Registro de sinal com ruído

            // Retransmite dados ao receptor
            _send(recipient, signal);
          }
        }
    }
  } catch (const std::runtime_error &e) {
    std::cout << e.what() << std::endl;
  }
}

void Channel::close() {
  // Fecha cada descritor de arquivo de socket
  for (auto fd : fds)
    ::close(fd.fd);

  // Fecha descritor de arquivo de visualização de sinal
  if (trace_fd >= 0)
    ::close(trace_fd);
  trace_fd = -1;

  // Deleta o arquivo de socket
  unlink(channel.sun_path);
}

void Channel::trace(TraceStage stage, int sender, const bytes &signal) {
  if (trace_fd < 0)
    return;

  // Constroi o cabeçalho do registro
  TraceHeader header = {};
  header.source_pid = peer_pid[sender];
  header.size = signal.size();
  header.time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count(); // Tempo em nanossegundos
  header.stage = stage;

  // Escrita de múltiplos buffers (cabeçalho e sinal) em arquivo de registro
  struct iovec parts[2] = {{&header, sizeof(header)},
                           {(void *)signal.data(), signal.size()}};
  if (writev(trace_fd, parts, 2) < 0)
    trace_fd = -1;
}

void Channel::add_noise(bytes &signal) {
  const size_t bufsize = signal.size();
  assert(bufsize % sizeof(double) == 0);

  // Interpreta buffer de bytes como ponto-flutuante (8 bytes)
  double *data = reinterpret_cast<double *>(signal.data());

  // Adiciona ruído gaussiano ao sinal
  for (double *D = data; D < data + bufsize / sizeof(double); D++)
    *D += dist(gn);
}

Channel::~Channel() { close(); }