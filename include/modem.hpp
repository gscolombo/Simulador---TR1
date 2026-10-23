#ifndef MODEM_H
#define MODEM_H

#include "types.hpp"
#include <cmath>
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

  double projection_coefficient(const signal_sample &g,
                                const signal_sample &x) const;
  double signal_energy(const signal_sample &y) const;
  double signal_correlation(const signal_sample &g,
                            const signal_sample &x) const;
  void estimate_df_offset(const signal_sample &sample);

  virtual void modulate(symbol &s, signal_sample &sample) const = 0;
  virtual void demodulate(signal_sample &sample, symbol &s) const = 0;

  double dc_offset = 0;

private:
  uint8_t rx_bits;
  uint64_t rx_acc;
  bytes sig_buf;

  int k;
  double *means, running_sum = 0;
  int means_counter = 0, m0 = 0;
};

class BasebandModem : public Modem {
public:
  BasebandModem(size_t sample_size, double bitrate, double A);
};

class NRZPolarModem : public BasebandModem {
public:
  using BasebandModem::BasebandModem;

  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<NRZPolarModem>(*this);
  }

private:
  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;
};

class ManchesterModem : public BasebandModem {
public:
  ManchesterModem(size_t sample_size, double bitrate, double A);

  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<ManchesterModem>(*this);
  }

private:
  std::vector<bit> clock;

  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;
};

class BipolarModem : public BasebandModem {
public:
  using BasebandModem::BasebandModem;

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
  ASKModem(size_t sample_size, double fc, double bitrate, double A);

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
  FSKModem(size_t sample_size, double fc, double bitrate, double A, double fd);

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

class QAM : public PassbandModem {
public:
  QAM(size_t symbol_size, size_t sample_size, double fc, double bitrate,
      double A);

  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;

protected:
  signal_sample I, Q;
  IQ_map constellation_map;
  static constexpr double inv_sqrt2 = 1 / std::numbers::sqrt2_v<double>;

  void set_IQ_signals();
};

class _8PSKModem : public QAM {
public:
  _8PSKModem(size_t sample_size, double fc, double bitrate, double A);

  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<_8PSKModem>(*this);
  }
};

class _32QAM : public QAM {
public:
  _32QAM(size_t sample_size, double fc, double bitrate, double A);

  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<_32QAM>(*this);
  }

protected:
  const double cos22 = cos(22.5 * (PI / 180));
  const double sin22 = sin(22.5 * (PI / 180));
};

} // namespace PhysicalLayer

#endif