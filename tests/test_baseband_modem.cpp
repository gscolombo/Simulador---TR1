#include "main.hpp"
#include "modem.hpp"
#include "types.hpp"

using namespace PhysicalLayer;

TEST_CASE(
    "Conversão dados ↔ volts em modem banda-base com codificação NRZ polar") {
  const size_t sample_size = 100;
  NRZPolarModem modem(1, sample_size, 1e3, 1.);
  bytes data{std::byte{0b10011110}};
  bytes volts;
  std::vector<double> expected_level{-1., 1., 1., 1., 1., -1., -1., 1.};

  modem.toVolts(data, volts);
  CHECK(volts.size() == 8 * sample_size * sizeof(double));

  const double *signal = reinterpret_cast<double *>(volts.data());
  for (size_t bit = 0; bit < 8; bit++)
    for (size_t n = 0; n < sample_size; n++)
      CHECK(signal[bit * sample_size + n] == expected_level[bit]);

  bytes recovered;
  modem.fromVolts(volts, recovered);
  CHECK(recovered == data);
}

TEST_CASE(
    "Conversão dados ↔ volts em modem banda-base com codificação Manchester") {
  const size_t sample_size = 100;
  ManchesterModem modem(1, sample_size, 1e3, 1.);
  bytes data{std::byte{0b10011110}};
  bytes volts;
  std::vector<std::pair<double, double>> expected_halves{
      {0., 1.}, {1., 0.}, {1., 0.}, {1., 0.},
      {1., 0.}, {0., 1.}, {0., 1.}, {1., 0.}};

  modem.toVolts(data, volts);
  CHECK(volts.size() == 8 * sample_size * sizeof(double));

  const double *signal = reinterpret_cast<double *>(volts.data());
  for (size_t bit = 0; bit < 8; bit++) {
    size_t base = bit * sample_size;
    for (size_t n = 0; n < sample_size / 2; n++)
      CHECK(signal[base + n] == expected_halves[bit].first);
    for (size_t n = sample_size / 2; n < sample_size; n++)
      CHECK(signal[base + n] == expected_halves[bit].second);
  }

  bytes recovered;
  modem.fromVolts(volts, recovered);
  CHECK(recovered.size() == 1);
  CHECK(recovered == data);
}

TEST_CASE(
    "Conversão dados ↔ volts em modem banda-base com codificação Bipolar") {
  const size_t sample_size = 100;
  BipolarModem modem(1, sample_size, 1e3, 1.);
  bytes data{std::byte{0b10011110}};
  bytes volts;
  std::vector<double> expected_level{0., 1., -1., 1., -1., 0., 0., 1.};

  modem.toVolts(data, volts);
  CHECK(volts.size() == 8 * sample_size * sizeof(double));

  const double *signal = reinterpret_cast<double *>(volts.data());
  for (size_t bit = 0; bit < 8; bit++)
    for (size_t n = 0; n < sample_size; n++)
      CHECK(signal[bit * sample_size + n] == expected_level[bit]);

  bytes recovered;
  modem.fromVolts(volts, recovered);
  CHECK(recovered == data);
}