#include "main.hpp"
#include "modem.hpp"
#include "types.hpp"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <utility>

using namespace PhysicalLayer;

TEST_CASE("PassbandModem::toVolts agrupa bits em símbolos e gera `sample_size` "
          "amostras cada e Passband::fromVolts retorna bytes a partir de "
          "amostras de tamanho `sample_size`") {
  constexpr size_t sample_size = 500;
  constexpr size_t fc = 2.4e9;
  constexpr size_t bitrate = 11e6;
  constexpr double A = 2.;

  SUBCASE("Modem ASK") {
    class TestASKModem : public ASKModem {
    public:
      using ASKModem::ASKModem;

      double get_threshold() { return threshold; }
      signal_sample *get_carrier() { return &carrier; }
    };

    SUBCASE("Cálculo de potência média para definição de limiar é correto.") {
      std::pair<double, double> standards[3]{
          {2.4e9, 11e6}, // 802.11b (RF: 2.4GHz, 11Mbps)
          {2.4e9, 54e6}, // 802.11g (RF: 2.4GHz, 54Mbps)
          {5e9, 54e6},   // 802.11a (RF: 5GHz, 54Mbps)
      };

      for (auto p : standards) {
        TestASKModem m(1, sample_size, p.first, p.second, 2.);
        CHECK(m.get_threshold() == doctest::Approx(1.).epsilon(0.01));
      }
    }

    SUBCASE("modulate(0) gera silêncio") {
      symbol s{0};
      signal_sample sample;

      TestASKModem m(1, sample_size, fc, bitrate, A);

      m.modulate(s, sample);

      CHECK(sample.size() == sample_size);
      for (double v : sample)
        CHECK(v == 0.);
    }

    SUBCASE("modulate(1) gera amplitude da portadora") {
      symbol s{1};
      signal_sample sample;

      TestASKModem m(1, sample_size, fc, bitrate, A);
      m.modulate(s, sample);

      CHECK(sample.size() == sample_size);
      for (size_t n = 0; n < sample_size; n++)
        CHECK(sample[n] ==
              doctest::Approx((*m.get_carrier())[n]).epsilon(1e-4));
    }

    SUBCASE("demodulate({0., ..., 0.}) retorna 0") {
      symbol s;
      signal_sample sample;

      sample.insert(sample.end(), sample_size, 0.);

      TestASKModem m(1, sample_size, fc, bitrate, A);
      m.demodulate(sample, s);

      CHECK(s.size() == 1);
      CHECK(s[0] == 0);
    }

    SUBCASE("demodulate com sinal senoidal retorna 1") {
      symbol s;
      signal_sample sample;

      TestASKModem m(1, sample_size, fc, bitrate, A);

      for (size_t n = 0; n < sample_size; n++)
        sample.push_back((*m.get_carrier())[n]);

      m.demodulate(sample, s);

      CHECK(s.size() == 1);
      CHECK(s[0] == 1);
    }
  }

  SUBCASE("Modem FSK") {
    class TestFSKModem : public FSKModem {
    public:
      using FSKModem::FSKModem;

      signal_sample *get_f1_carrier() { return &f1_carrier; };
      signal_sample *get_f2_carrier() { return &f2_carrier; };
    };

    TestFSKModem m(1, sample_size, fc, bitrate, A, 1e3);

    SUBCASE("modulate(0) gera a frequência f0 = fc - fd") {
      symbol s{0};
      signal_sample sample;
      m.modulate(s, sample);

      CHECK(sample.size() == sample_size);
      for (size_t n = 0; n < sample_size; n++)
        CHECK(sample[n] ==
              doctest::Approx((*m.get_f1_carrier())[n]).epsilon(1e-4));
    }

    SUBCASE("modulate(1) gera a frequência f1 = fc + fd") {
      symbol s{1};
      signal_sample sample;
      m.modulate(s, sample);

      CHECK(sample.size() == sample_size);
      for (size_t n = 0; n < sample_size; n++)
        CHECK(sample[n] ==
              doctest::Approx((*m.get_f2_carrier())[n]).epsilon(1e-4));
    }

    SUBCASE("demodulate correlaciona melhor com f0 do que f1, decide bit 0") {
      symbol s{0}, recovered;
      signal_sample sample;
      m.modulate(s, sample);
      m.demodulate(sample, recovered);

      CHECK(recovered.size() == 1);
      CHECK(recovered[0] == 0);
    }

    SUBCASE("demodulate correlaciona melhor com f1 do que f0, decide bit 1") {
      symbol s{1}, recovered;
      signal_sample sample;
      m.modulate(s, sample);
      m.demodulate(sample, recovered);

      CHECK(recovered.size() == 1);
      CHECK(recovered[0] == 1);
    }
  }

  SUBCASE("Modem 8PSK") {
    class Test8PSKModem : public _8PSKModem {
    public:
      using _8PSKModem::_8PSKModem;

      auto get_constellation() { return constellation_map; }
      double correlate(signal_sample g, signal_sample x) {
        return cross_correlation(g, x);
      }
    };

    Test8PSKModem m(3, sample_size, fc, bitrate * 3, A);

    SUBCASE("Constelação tem 8 pontos distintos, todos com mesma amplitude") {
      auto c = m.get_constellation();
      auto amp = [](std::pair<double, double> c) {
        return sqrt(c.first * c.first + c.second * c.second);
      };

      double ref = amp(c[0].second);

      for (int i = 0; i < 8; i++) {
        double v = amp(c[i].second);
        CHECK(v == doctest::Approx(ref).epsilon(1e-6));
      }
    }

    SUBCASE(
        "Símbolos adjacentes na constelação são distinguíveis por correlação") {
      // Verifica se a correlação cruzada entre símbolos distintos é sempre
      // menor que a autocorrelação, necessário para que a demodulação
      // consiga discriminar entre eles.

      for (int i = 0; i < 8; i++) {
        symbol si;

        for (int b = 0; b < 3; b++)
          si.push_back((i >> b) & 1);

        signal_sample sample_i;
        m.modulate(si, sample_i);
        double auto_p = m.correlate(sample_i, sample_i);

        for (int j = 0; j < 8; j++) {
          if (i == j)
            continue;
          symbol sj;
          for (int b = 0; b < 3; b++)
            sj.push_back((j >> b) & 1);
          signal_sample sample_j;
          m.modulate(sj, sample_j);

          double p = m.correlate(sample_i, sample_j);
          CHECK(p < auto_p);
        }
      }
    }

    SUBCASE("Roundtrip: cada um dos 8 símbolos é recuperado corretamente") {
      for (int val = 0; val < 8; val++) {
        symbol s, recovered;
        for (int b = 2; b >= 0; b--)
          s.push_back((val >> b) & 1);

        signal_sample sample;
        m.modulate(s, sample);

        CHECK(sample.size() == sample_size);

        m.demodulate(sample, recovered);

        CHECK(recovered.size() == 3);
        CHECK(recovered == s);
      }
    }
  }

  SUBCASE("Roundtrip: toVolts seguido de fromVolts recupera os dados originais "
          "para diversos tamanhos e padrões de mensagem") {
    const char *signal_debug_path = "/tmp/signal_debug";

    constexpr double fc = 2.4e9;
    constexpr double bitrate = 11e6;
    constexpr double A = 2.;

    ASKModem ask(1, sample_size, fc, bitrate, A);
    FSKModem fsk(1, sample_size, fc, bitrate, A, 1e3);
    _8PSKModem _8psk(3, sample_size, fc, bitrate * 3, A);

    std::filesystem::create_directory(signal_debug_path);

    PassbandModem *modems[3]{&ask, &fsk, &_8psk};

    for (PassbandModem *modem : modems) {
      auto roundtrip = [&modem](const bytes &original) {
        bytes volts, recovered;
        modem->reset = true;
        modem->toVolts(original, volts);
        modem->fromVolts(volts, recovered);
        CHECK(recovered == original);
      };

      SUBCASE("Mensagem vazia") { roundtrip({}); }

      SUBCASE("Um único byte, valores extremos e intermediários") {
        roundtrip({std::byte{0x00}});
        roundtrip({std::byte{0xFF}});
        roundtrip({std::byte{0b10101010}});
        roundtrip({std::byte{0b01010101}});
      }

      SUBCASE("Múltiplos bytes, tamanho não múltiplo de symbol_size") {
        // 2 bytes = 16 bits; com symbol_size=3, não fecha exato (16 % 3 != 0)
        // -> exercita padding
        roundtrip({std::byte{0b00000111}, std::byte{0b10101010}});

        // 3 bytes = 24 bits; com symbol_size=3, fecha exato (24 % 3 == 0) ->
        // sem padding
        roundtrip({std::byte{0xFF}, std::byte{0x00}, std::byte{0xAA}});

        // 5 bytes, mistura de padrões
        roundtrip({std::byte{0x01}, std::byte{0x80}, std::byte{0x55},
                   std::byte{0xAA}, std::byte{0xFF}});
      }

      SUBCASE("Mensagem longa, gerada deterministicamente") {
        bytes original;
        // padrão pseudo-aleatório determinístico (LCG simples), sem depender
        // de <random>
        uint32_t seed = 12345;
        for (int i = 0; i < 64; i++) {
          seed = seed * 1103515245 + 12345;
          original.push_back(std::byte{(uint8_t)(seed >> 16)});
        }
        roundtrip(original);
      }

      SUBCASE("Todos os bytes possíveis, um roundtrip por valor") {
        for (int v = 0; v <= 0xFF; v++)
          roundtrip({std::byte{(uint8_t)v}});
      }

      SUBCASE("Mensagem de texto") {

        std::string message{"Hello World!"};
        bytes message_bytes(message.size());
        memcpy(message_bytes.data(), message.data(), message.size());

        roundtrip(message_bytes);
      }
    }
  }
}