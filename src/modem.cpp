/**
 * @file modem.cpp
 * @brief Implementação dos modems de banda-base e banda passante.
 * @details
 * A implementação converte grupos de bits em amostras de tensão e aplica a
 * demodulação correspondente no receptor. As classes de banda-base usam
 * pulsos diretamente, enquanto as classes de banda passante usam portadoras
 * e, quando aplicável, constelações em coordenadas I/Q.
 */

#include "modem.hpp"
#include "types.hpp"
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <vector>

using namespace PhysicalLayer;

Modem::Modem(size_t symbol_size, size_t sample_size, double bitrate, double A)
    : symbol_size(symbol_size), sample_size(sample_size), bitrate(bitrate),
      A(A) {
  baud = bitrate / (double)(symbol_size);
  k = sample_size * 10;
  means = (double *)malloc(sizeof(double) * k);
}

void Modem::toVolts(const bytes &data, bytes &volts) {
  bytes::const_iterator B;
  uint64_t acc = 0x0;
  size_t bits = 0;
  symbol s;
  std::vector<double> sample;
  std::byte *sample_data;

  auto process_bits = [&acc, &bits, &s, &sample, &sample_data, &volts, this] {
    s.clear();
    sample.clear();

    while (s.size() < this->symbol_size) {
      s.push_back((bool)(acc & 1));
      acc >>= 1;
    }

    this->modulate(s, sample);
    assert(sample.size() == sample_size);

    sample_data = reinterpret_cast<std::byte *>(sample.data());

    volts.insert(volts.end(), sample_data,
                 sample_data + sample.size() * sizeof(double));
    bits -= this->symbol_size;
  };

  for (B = data.begin(); B != data.end(); B++) {
    acc |= (u_int64_t)*B << bits;
    bits += 8;
    while (bits >= symbol_size)
      process_bits();
  }

  if (bits > 0)
    process_bits();
}

void Modem::fromVolts(const bytes &volts, bytes &data) {
  // volts deve ser um vetor de bytes representando
  assert(volts.size() % sizeof(double) == 0);

  signal_sample sample;
  symbol s;

  static const size_t min_size = sample_size * sizeof(double);

  if (reset) {
    rx_bits = 0;
    rx_acc = 0x0;
    sig_buf.clear();
    reset = false;
  }

  sig_buf.insert(sig_buf.end(), volts.begin(), volts.end());

  if (sig_buf.size() < min_size)
    return;

  // Reinterpreta bytes de sinal elétrico em doubles
  const double *signal = reinterpret_cast<const double *>(sig_buf.data());

  // Para cada conjunto de `sample_size` doubles...
  size_t processed = 0, total = (sig_buf.size() / sizeof(double));

  while (processed + sample_size <= total) {
    sample.clear();
    sample.insert(sample.end(), signal + processed,
                  signal + processed + sample_size);

    // Demodula o conjunto de doubles no símbolo correspondente
    s.clear();
    estimate_df_offset(sample);
    demodulate(sample, s);
    assert(s.size() == symbol_size);

    // Concatena o símbolo no acumulador
    size_t i = 0;
    while (i < symbol_size) {
      rx_acc |= (uint64_t)s[i] << (rx_bits + i);
      i++;
    }

    rx_bits += symbol_size;

    // Se 8 bits foram acumulados, insere no vetor de bytes de dados
    if (rx_bits >= 8) {
      data.push_back((std::byte)(rx_acc & 0x00FF));
      rx_acc >>= 8;
      rx_bits -= 8;
    }

    processed += sample_size;
  }

  sig_buf.erase(sig_buf.begin(),
                sig_buf.begin() + (processed * sizeof(double)));
}

double Modem::signal_energy(const signal_sample &y) const {
  assert(y.size() == this->sample_size);

  double E = 0.;

  // Cada instante de tempo corresponde a uma amostra de um símbolo
  const double dt = 1. / (this->sample_size * this->baud);

  for (double v : y)
    E += pow(v, 2) * dt;

  return E;
}

double Modem::projection_coefficient(const signal_sample &g,
                                     const signal_sample &x) const {
  assert(g.size() == x.size());

  double Ex = signal_energy(x);
  double p = 0.;
  double dt = 1. / (this->sample_size * this->baud);

  for (size_t n = 0; n < g.size(); n++)
    p += g[n] * x[n] * dt;

  return p / Ex; // <g(t),x(t)>/||x(t)||²
}

double Modem::signal_correlation(const signal_sample &g,
                                 const signal_sample &x) const {
  double Ex = signal_energy(x);
  double Eg = signal_energy(g);

  return projection_coefficient(g, x) *
         sqrt(Ex / Eg); // <g(t),x(t)>/(||g(t)|| * ||x(t)||)
}

void Modem::estimate_df_offset(const signal_sample &sample) {
  double m = 0;
  for (double s : sample)
    m += s;
  m /= sample.size();

  dc_offset = means_counter ? running_sum / means_counter : 0.;

  if (means_counter == k)
    running_sum -= means[m0];
  else
    means_counter++;

  means[m0] = m;
  m0 = (m0 + 1) % k;
  running_sum += m;
}

/* Modems banda-base (NRZ Polar, Manchester e Bipolar) */
BasebandModem::BasebandModem(size_t sample_size, double bitrate, double A)
    : Modem(1, sample_size, bitrate, A) {}

/* ===== NRZ ===== */

void NRZPolarModem::modulate(symbol &s, signal_sample &sample) const {
  assert(s.size() == 1);
  assert(s[0] == 0 || s[0] == 1);

  auto set_pulse = [&sample, this](double v) {
    sample.insert(sample.end(), this->sample_size, v);
  };

  if (!s[0])
    set_pulse(-A);
  else
    set_pulse(A);
}
void NRZPolarModem::demodulate(signal_sample &sample, symbol &s) const {
  assert(sample.size() == sample_size);

  double mean = 0;
  for (double s : sample)
    mean += s;
  mean /= sample.size();

  s.push_back(mean > dc_offset ? 1 : 0);
};

/* ===== Manchester ===== */

ManchesterModem::ManchesterModem(size_t sample_size, double bitrate, double A)
    : BasebandModem(sample_size, bitrate, A) {
  assert(sample_size % 2 == 0);

  // Inicializa amostra de sinal de clock
  clock.insert(clock.end(), (size_t)(sample_size / 2.), 0);
  clock.insert(clock.end(), (size_t)(sample_size / 2.), 1);
}

void ManchesterModem::modulate(symbol &s, signal_sample &sample) const {
  assert(s.size() == 1);
  assert(s[0] == 0 || s[0] == 1);

  for (size_t i = 0; i < sample_size; i++)
    sample.push_back((double)(s[0] ^ clock[i]) * A);
}

void ManchesterModem::demodulate(signal_sample &sample, symbol &s) const {
  assert(sample.size() == sample_size);

  double pulse_mean = 0;

  for (size_t i = 0; i < sample_size; i++)
    pulse_mean += ((bit)(sample[i] > dc_offset) ^ clock[i]);
  pulse_mean /= sample_size;

  s.push_back((bit)(pulse_mean > 0.5));
}

/* ===== Bipolar ===== */

void BipolarModem::modulate(symbol &s, signal_sample &sample) const {
  assert(s.size() == 1);
  assert(s[0] == 0 || s[0] == 1);

  positive ^= s[0];
  sample.insert(sample.end(), sample_size,
                s[0] ? (positive ? 1.0 : -1.0) : 0.0);
}
void BipolarModem::demodulate(signal_sample &sample, symbol &s) const {
  assert(sample.size() == sample_size);

  double m = 0;
  for (double s : sample)
    m += s;
  m /= sample.size();

  s.push_back((fabs(m - dc_offset)) > 0.5 ? 1 : 0);
}

/* =================== */

/**
  Modems de banda passante
  - ASK
  - FSK
  - PSK
  - 32-QAM
*/

PassbandModem::PassbandModem(size_t symbol_size, size_t sample_size, double fc,
                             double bitrate, double A)
    : Modem(symbol_size, sample_size, bitrate, A), fc(fc) {
  /**
    Para satisfazer a taxa de Nyquist, a frequência de amostragem no receptor
    deve ser maior que 2 vezes a frequência máxima do sinal transmitido.
    Nesse caso, a taxa de amostragem corresponde ao tamanho da amostra
    necessária para demodular um símbolo (i.e., 1/(sample_size * baud_rate)).

    Além disso, pela finalidade de uma onda "portadora", espera-se que o baud
    seja consideravelmente menor que a frequência da portadora.
  */

  assert(sample_size > 2 * (fc / baud));
  assert(fc >= baud * 10.);
}

ASKModem::ASKModem(size_t sample_size, double fc, double bitrate, double A)
    : PassbandModem(1, sample_size, fc, bitrate, A) {
  // Calcula o sinal da portadora
  for (size_t n = 0; n < sample_size; n++) {
    double t = (double)n / (sample_size * baud);
    carrier.push_back(A * cos(2 * PI * fc * t));
  }

  /*
    Calcula a potência média da portadora e utiliza a metade do valor
    como limiar entre o nível alto (bit 1) e baixo (0).
  */
  threshold = (signal_energy(carrier) * baud) / 2.;
}

void ASKModem::modulate(symbol &s, signal_sample &sample) const {
  assert(s.size() == 1);
  assert(s[0] == 0 || s[0] == 1);

  if (!s[0])
    sample.insert(sample.end(), sample_size, 0.);
  else
    sample.insert(sample.end(), carrier.begin(), carrier.end());
};

void ASKModem::demodulate(signal_sample &sample, symbol &s) const {
  assert(sample.size() == sample_size);

  for (double *s = sample.data(); s != sample.data() + sample.size(); s++)
    *s -= dc_offset;

  double p = signal_energy(sample) * baud;

  s.push_back((bit)(p > threshold));
};

FSKModem::FSKModem(size_t sample_size, double fc, double bitrate, double A,
                   double fd)
    : PassbandModem(1, sample_size, fc, bitrate, A), fd(fd) {
  /*
    Calcula as portadoras de frequências fc ± fd
  */
  double f1 = fc - fd;
  double f2 = fc + fd;

  for (size_t n = 0; n < sample_size; n++) {
    double t = (double)n / (sample_size * baud);
    f1_carrier.push_back(A * cos(2 * PI * f1 * t));
    f2_carrier.push_back(A * cos(2 * PI * f2 * t));
  }
}

void FSKModem::modulate(symbol &s, signal_sample &signal) const {
  assert(s.size() == 1);
  assert(s[0] == 0 || s[0] == 1);

  if (!s[0])
    signal.insert(signal.end(), f1_carrier.begin(), f1_carrier.end());
  else
    signal.insert(signal.end(), f2_carrier.begin(), f2_carrier.end());
}

void FSKModem::demodulate(signal_sample &sample, symbol &s) const {
  assert(sample.size() == sample_size);

  double p1 = signal_correlation(f1_carrier, sample);
  double p2 = signal_correlation(f2_carrier, sample);

  s.push_back((bit)(p2 >= p1));
}

QAM::QAM(size_t symbol_size, size_t sample_size, double fc, double bitrate,
         double A)
    : PassbandModem(symbol_size, sample_size, fc, bitrate, A) {
  set_IQ_signals();
}

void QAM::set_IQ_signals() {
  for (size_t n = 0; n < sample_size; n++) {
    double ang = 2 * PI * fc * ((double)n / (sample_size * baud));
    I.push_back(A * cos(ang));
    Q.push_back(A * sin(ang));
  }
}

void QAM::modulate(symbol &s, signal_sample &sample) const {
  assert(s.size() == symbol_size);

  char o = 0b000;

  for (size_t i = 0; i < symbol_size; i++) {
    char b = s[i];
    assert(b == 0 || b == 1);
    o |= b << i;
  }

  IQ_coord iq;
  for (auto p : constellation_map)
    if (p.first == o) {
      iq = p.second;
      break;
    }

  for (size_t n = 0; n < sample_size; n++) {
    sample.push_back(iq.first * I[n] + iq.second * Q[n]);
  }
}

void QAM::demodulate(signal_sample &sample, symbol &s) const {
  assert(sample.size() == sample_size);

  double pi = projection_coefficient(sample, I);
  double pq = projection_coefficient(sample, Q);

  auto dist = [](IQ_coord a, IQ_coord b) {
    return sqrt(pow(a.first - b.first, 2) + pow(a.second - b.second, 2));
  };

  auto set_symbol = [&s, this](char _s) {
    s.clear();
    for (size_t i = 0; i < this->symbol_size; i++)
      s.push_back((_s >> i) & 0b001);
  };

  double min_dist = INFINITY, d = 0;
  for (auto p : constellation_map) {
    d = dist(p.second, {pi, pq});
    if (d < min_dist) {
      min_dist = d;
      set_symbol(p.first);
    }
  }
}

_8PSKModem::_8PSKModem(size_t sample_size, double fc, double bitrate, double A)
    : QAM(3, sample_size, fc, bitrate, A) {
  constellation_map = {{0b000, {1., 0.}},  {0b001, {inv_sqrt2, inv_sqrt2}},
                       {0b101, {0., 1}},   {0b100, {-inv_sqrt2, inv_sqrt2}},
                       {0b110, {-1., 0.}}, {0b111, {-inv_sqrt2, -inv_sqrt2}},
                       {0b011, {0., -1}},  {0b010, {inv_sqrt2, -inv_sqrt2}}};
}

_32QAM::_32QAM(size_t sample_size, double fc, double bitrate, double A)
    : QAM(5, sample_size, fc, bitrate, A) {
  constellation_map = {// r = 1
                       {0b00000, {1., 0.}},
                       {0b00001, {inv_sqrt2, inv_sqrt2}},
                       {0b00010, {-inv_sqrt2, inv_sqrt2}},
                       {0b00011, {0., 1.}},
                       {0b00100, {inv_sqrt2, -inv_sqrt2}},
                       {0b00101, {0., -1.}},
                       {0b00110, {-1., 0.}},
                       {0b00111, {-inv_sqrt2, -inv_sqrt2}},
                       // r = 2
                       {0b01000, {2 * cos22, 2 * sin22}},
                       {0b01001, {2 * sin22, 2 * cos22}},
                       {0b01011, {-2 * sin22, 2 * cos22}},
                       {0b01010, {-2 * cos22, 2 * sin22}},
                       {0b01110, {-2 * cos22, -2 * sin22}},
                       {0b01111, {-2 * sin22, -2 * cos22}},
                       {0b01101, {2 * sin22, -2 * cos22}},
                       {0b01100, {2 * cos22, -2 * sin22}},
                       // r = 3
                       {0b11000, {3., 0.}},
                       {0b11001, {3 * inv_sqrt2, 3 * inv_sqrt2}},
                       {0b11011, {0., 3.}},
                       {0b11010, {-3 * inv_sqrt2, 3 * inv_sqrt2}},
                       {0b11110, {-3., 0.}},
                       {0b11111, {-3 * inv_sqrt2, -3 * inv_sqrt2}},
                       {0b11101, {0., -3.}},
                       {0b11100, {3 * inv_sqrt2, -3 * inv_sqrt2}},
                       // r = 4
                       {0b10000, {4 * cos22, 4 * sin22}},
                       {0b10001, {4 * sin22, 4 * cos22}},
                       {0b10011, {-4 * sin22, 4 * cos22}},
                       {0b10010, {-4 * cos22, 4 * sin22}},
                       {0b10110, {-4 * cos22, -4 * sin22}},
                       {0b10111, {-4 * sin22, -4 * cos22}},
                       {0b10101, {4 * sin22, -4 * cos22}},
                       {0b10100, {4 * cos22, -4 * sin22}}};
}