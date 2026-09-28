#ifndef MODEM_H
#define MODEM_H

#include "types.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <sys/types.h>

#define PI std::numbers::pi

namespace PhysicalLayer {

class Modem {
public:
  Modem(size_t symbol_size, size_t sample_size, double bitrate, double A);
  ~Modem() = default;

  virtual std::shared_ptr<Modem> clone() const = 0;

  void toVolts(const bytes &data, bytes &volts);
  void fromVolts(const bytes &volts, bytes &data);

  bool reset = true;

protected:
  size_t symbol_size;
  size_t sample_size;
  double bitrate;
  double baud;
  double A;

  double cross_correlation(const signal_sample &g,
                           const signal_sample &x) const;
  double signal_energy(const signal_sample &y) const;

  virtual void modulate(symbol &s, signal_sample &sample) const = 0;
  virtual void demodulate(signal_sample &sample, symbol &s) const = 0;

private:
  uint8_t rx_bits;
  uint64_t rx_acc;
  bytes sig_buf;
};

class NRZPolarModem : public Modem {
public:
  using Modem::Modem;

  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<NRZPolarModem>(*this);
  }

private:
  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;
};

class ManchesterModem : public Modem {
public:
  ManchesterModem(size_t symbol_size, size_t sample_size, double bitrate,
                  double A);

  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<ManchesterModem>(*this);
  }

private:
  std::vector<bit> clock;

  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;
};

class BipolarModem : public Modem {
public:
  using Modem::Modem;

  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<BipolarModem>(*this);
  }

private:
  mutable bool positive = false;
  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;
};

/* Modems banda passante (ASK, FSK, 8PSK e 32-QAM) */

class PassbandModem : public Modem {
public:
  PassbandModem(size_t symbol_size, size_t sample_size, double fc,
                double bitrate, double A);

protected:
  double fc;
};

class ASKModem : public PassbandModem {
public:
  ASKModem(size_t symbol_size, size_t sample_size, double fc, double bitrate,
           double A);

  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<ASKModem>(*this);
  }

  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;

protected:
  double threshold;
  signal_sample carrier;
};

class FSKModem : public PassbandModem {
public:
  FSKModem(size_t symbol_size, size_t sample_size, double fc, double bitrate,
           double A, double fd);

  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<FSKModem>(*this);
  }

  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;

protected:
  double fd;
  signal_sample f1_carrier; // fc - fd
  signal_sample f2_carrier; // fc + fd
};

class _8PSKModem : public PassbandModem {
public:
  _8PSKModem(size_t symbol_size, size_t sample_size, double fc, double bitrate,
             double A);

  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<_8PSKModem>(*this);
  }

  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;

protected:
  IQ_map<8> constellation_map;
  signal_sample I, Q;

private:
  static constexpr double inv_sqrt2 = 1 / std::numbers::sqrt2_v<double>;
};

} // namespace PhysicalLayer

#endif