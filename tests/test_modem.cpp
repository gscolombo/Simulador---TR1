#include "main.hpp"
#include "modem.hpp"
#include "types.hpp"
#include <cmath>
#include <cstring>
#include <memory>

using namespace PhysicalLayer;

class TestModem : public Modem {
public:
  using Modem::Modem;

  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<TestModem>(*this);
  }

  double test_cross_correlation(signal_sample g, signal_sample x) {
    return cross_correlation(g, x);
  }

protected:
  void modulate(symbol &s, signal_sample &sample) const override {
    double value = 0;
    for (size_t i = 0; i < s.size(); i++)
      value += s[i] ? (1 << i) : 0;

    sample.assign(sample_size, value);
  }

  void demodulate(signal_sample &sample, symbol &s) const override {
    double value = sample.front();
    s.resize(symbol_size);

    for (size_t i = 0; i < symbol_size; i++) {
      s[i] = ((int)value) & 1;
      value = ((int)value) >> 1;
    }
  }
};

TEST_CASE("Modem::toVolts agrupa bits em símbolos e gera `sample_size` "
          "amostras cada e Modem::fromVolts retorna bytes a partir de "
          "amostras de tamanho `sample_size`") {
  constexpr size_t symbol_size = 3;
  constexpr size_t sample_size = 500;
  constexpr size_t bitrate = 204e3;

  TestModem modem(symbol_size, sample_size, bitrate, 1.);

  const bytes data{std::byte{0b00000111}};

  SUBCASE("Conversão de byte único, com padding no último símbolo") {
    bytes volts;

    modem.toVolts(data, volts);

    CHECK(volts.size() ==
          ceil(8. / symbol_size) * sample_size * sizeof(double));

    double *samples = reinterpret_cast<double *>(volts.data());

    size_t n = 0;
    // símbolo 1: bits 0,1,2 = 0b111 = 7 -> amplitude 7
    while (n < sample_size)
      CHECK(samples[n++] == 7.0);

    // símbolo 2: bits 3,4,5 = 0b000 = 0 -> amplitude 0, todas amostras 0
    n = 0;
    while (n < sample_size)
      CHECK(samples[sample_size + n++] == 0.0);

    // símbolo 3: bits 6,7 + padding = 0b000 = 0 -> amplitude 0
    n = 0;
    while (n < sample_size)
      CHECK(samples[2 * sample_size + n++] == 0.0);
  }

  SUBCASE("Conversão de amostras de tamanho `sample_size` em vetor de bytes "
          "com um elemento.") {
    bytes volts, result;
    volts.resize(ceil(8. / symbol_size) * sample_size * sizeof(double));
    double *samples = reinterpret_cast<double *>(volts.data());

    size_t n = 0;
    while (n < sample_size)
      samples[n++] = 7.0;

    n = 0;
    while (n < sample_size)
      samples[n++ + sample_size] = 0.0;

    n = 0;
    while (n < sample_size)
      samples[n++ + sample_size * 2] = 0.0;

    modem.fromVolts(volts, result);
    CHECK(result.size() == 1);
    CHECK(result == data);
  }

  SUBCASE("Mensagem vazia não gera amostras") {
    bytes volts;

    modem.toVolts({}, volts);

    CHECK(volts.empty());
  }

  SUBCASE("Correlação calcula similaridade esperada entre sinais") {
    double e = 1e-6;

    SUBCASE("Similaridade máxima (ρ = 1)") {
      signal_sample g;
      for (size_t t = 0; t < sample_size; t++)
        g.push_back(static_cast<double>(t) * 1.37 -
                    5.0); // sinal arbitrário, não trivial

      CHECK(modem.test_cross_correlation(g, g) ==
            doctest::Approx(1.).epsilon(e));
    }

    SUBCASE("Nenhuma relação (ρ = 0)") {
      // g constante e x uma rampa centrada em zero são exatamente ortogonais
      // por construção, para qualquer sample_size (soma de x[t] = 0 sempre)
      signal_sample g(sample_size, 1.0);
      signal_sample x;
      double mean = (sample_size - 1) / 2.0;
      for (size_t t = 0; t < sample_size; t++)
        x.push_back(static_cast<double>(t) - mean);

      CHECK(modem.test_cross_correlation(g, x) ==
            doctest::Approx(0.).epsilon(e));
    }

    SUBCASE("Similaridade mínima (ρ = -1)") {
      signal_sample g;
      for (size_t t = 0; t < sample_size; t++)
        g.push_back(static_cast<double>(t) * 1.37 - 5.0);

      signal_sample x;
      for (double v : g)
        x.push_back(-v);

      CHECK(modem.test_cross_correlation(g, x) ==
            doctest::Approx(-1.).epsilon(e));
    }
  }
}