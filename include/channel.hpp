#ifndef CHANNEL_H
#define CHANNEL_H

#include "types.hpp"
#include <random>
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

protected:
private:
  int channel_sock;
  struct pollfd fds[3];
  double noise_mu;
  double noise_sigma;
  std::mt19937 gn;
  std::normal_distribution<double> dist;

  void add_noise(bytes &signal);
};

} // namespace PhysicalLayer

#endif