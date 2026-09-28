#ifndef TRANSCEIVER_H
#define TRANSCEIVER_H

#include "modem.hpp"
#include "types.hpp"
#include <atomic>
#include <deque>
#include <sys/types.h>
#include <sys/un.h>

namespace PhysicalLayer {

class Transceiver {
public:
  std::atomic<bool> tx_ready = false, rx_ready = false;
  std::deque<bytes> buffer;

  Transceiver(std::shared_ptr<Modem> modem);

  void connect();
  void shutdown();
  void transmit(const bytes &message);
  ssize_t receive();

  inline void reset_modem() { modem->reset = true; }

  ~Transceiver();

private:
  int channel_sock;
  std::shared_ptr<Modem> modem;
};

} // namespace PhysicalLayer

#endif