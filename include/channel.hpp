#ifndef CHANNEL_H
#define CHANNEL_H

#define INIT_POLLFD {-1, POLLIN, 0}

#include "gui/signal_trace.hpp"
#include "types.hpp"
#include <random>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>

namespace PhysicalLayer {

#define CHANNEL_DEBUG true

class Channel {
public:
  Channel(double noiseMu, double noiseSigma);
  ~Channel();

  void open();
  void stream();
  void close();

  // Habilita a gravação do sinal (antes e depois do ruído) em arquivo, no
  // formato descrito em signal_trace.hpp.
  void set_trace_file(const std::string &path) { trace_path = path; }

protected:
private:
  int channel_sock;
  struct pollfd fds[3] = {INIT_POLLFD, INIT_POLLFD, INIT_POLLFD};
  double noise_mu;
  double noise_sigma;
  std::mt19937 gn;
  std::normal_distribution<double> dist;

  std::string trace_path;
  int trace_fd = -1;
  uint32_t peer_pid[3] = {0, 0, 0}; // PID do processo conectado em cada fds[i]

  void add_noise(bytes &signal);
  void trace(TraceStage stage, int sender, const bytes &signal);
};

} // namespace PhysicalLayer

#endif