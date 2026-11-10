/**
 * @file modem.hpp
 * @brief Modems de banda-base e banda passante da camada física.
 */
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

/**
 * @brief Interface comum para modulação e demodulação digital.
 * @details
 * O modem transforma bytes em amostras elétricas de tensão e realiza a
 * operação inversa no receptor. Cada símbolo contém `symbol_size` bits e é
 * representado por `sample_size` amostras. As amostras são serializadas como
 * valores `double` dentro de `bytes` para atravessar o canal físico.
 */
class Modem {
public:
  /**
   * @brief Configura os parâmetros de um modem.
   * @param symbol_size Quantidade de bits por símbolo.
   * @param sample_size Quantidade de amostras por símbolo.
   * @param bitrate Taxa de bits em bits por segundo.
   * @param A Amplitude nominal do sinal elétrico.
   */
  Modem(size_t symbol_size, size_t sample_size, double bitrate, double A);
  /** Destrutor virtual da interface de modem. */
  ~Modem() = default;

  /** Cria uma cópia polimórfica do modem. */
  virtual std::shared_ptr<Modem> clone() const = 0;

  /** Modula bytes em amostras elétricas serializadas. */
  void toVolts(const bytes &data, bytes &volts);
  /** Demodula amostras elétricas e acrescenta os bytes recuperados. */
  void fromVolts(const bytes &volts, bytes &data);

  /** Solicita a reinicialização do estado acumulado do receptor. */
  bool reset = true;

protected:
  /** Quantidade de bits codificados em cada símbolo. */
  size_t symbol_size;
  /** Quantidade de amostras usadas para representar cada símbolo. */
  size_t sample_size;
  /** Taxa de transmissão de bits, em bits por segundo. */
  double bitrate;
  /** Taxa de símbolos, calculada a partir de `bitrate`. */
  double baud;
  /** Amplitude nominal das amostras elétricas. */
  double A;

  /** Calcula o coeficiente de projeção de um sinal sobre outro. */
  double projection_coefficient(const signal_sample &g,
                                const signal_sample &x) const;
  /** Calcula a energia de um símbolo amostrado. */
  double signal_energy(const signal_sample &y) const;
  /** Calcula a correlação normalizada entre dois sinais. */
  double signal_correlation(const signal_sample &g,
                            const signal_sample &x) const;
  /** Estima e atualiza o deslocamento DC observado no sinal recebido. */
  void estimate_df_offset(const signal_sample &sample);

  /** Converte um símbolo de bits em amostras elétricas. */
  virtual void modulate(symbol &s, signal_sample &sample) const = 0;
  /** Recupera um símbolo de bits a partir de amostras elétricas. */
  virtual void demodulate(signal_sample &sample, symbol &s) const = 0;

  /** Estimativa atual do deslocamento contínuo do sinal recebido. */
  double dc_offset = 0;

private:
  /** Quantidade de bits já acumulados durante a demodulação. */
  uint8_t rx_bits;
  /** Acumulador dos bits recebidos antes de formar um byte. */
  uint64_t rx_acc;
  /** Fragmentos de sinal ainda não processados em símbolos completos. */
  bytes sig_buf;

  /** Tamanho da janela usada na estimativa do deslocamento DC. */
  int k;
  /** Valores médios armazenados na janela de estimativa. */
  double *means, running_sum = 0;
  /** Quantidade de médias atualmente preenchidas na janela. */
  int means_counter = 0, m0 = 0;
};

/** @brief Base para modems que transmitem diretamente em banda-base. */
class BasebandModem : public Modem {
public:
  /** Configura um modem de banda-base de um bit por símbolo. */
  BasebandModem(size_t sample_size, double bitrate, double A);
};

/** @brief Modulação NRZ polar, usando amplitudes positiva e negativa. */
class NRZPolarModem : public BasebandModem {
public:
  /** Reutiliza o construtor do modem de banda-base. */
  using BasebandModem::BasebandModem;

  /** Cria uma cópia polimórfica do modem NRZ polar. */
  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<NRZPolarModem>(*this);
  }

private:
  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;
};

/** @brief Modulação Manchester com transição no meio do símbolo. */
class ManchesterModem : public BasebandModem {
public:
  /** Configura o modem Manchester e sua sequência de clock. */
  ManchesterModem(size_t sample_size, double bitrate, double A);

  /** Cria uma cópia polimórfica do modem Manchester. */
  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<ManchesterModem>(*this);
  }

private:
  std::vector<bit> clock;

  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;
};

/** @brief Modulação bipolar com alternância de polaridade para bits 1. */
class BipolarModem : public BasebandModem {
public:
  /** Reutiliza o construtor do modem de banda-base. */
  using BasebandModem::BasebandModem;

  /** Cria uma cópia polimórfica do modem bipolar. */
  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<BipolarModem>(*this);
  }

private:
  mutable bool positive = false;

  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;
};

/** @brief Base para modems que usam uma frequência portadora. */
class PassbandModem : public Modem {
public:
  /**
   * @brief Configura um modem de banda passante.
   * @param symbol_size Quantidade de bits por símbolo.
   * @param sample_size Quantidade de amostras por símbolo.
   * @param fc Frequência da portadora, em hertz.
   * @param bitrate Taxa de bits, em bits por segundo.
   * @param A Amplitude da portadora.
   */
  PassbandModem(size_t symbol_size, size_t sample_size, double fc,
                double bitrate, double A);

protected:
  /** Frequência da portadora, em hertz. */
  double fc;
};

/** @brief Modulação ASK, alterando a amplitude da portadora. */
class ASKModem : public PassbandModem {
public:
  /** Configura um modem ASK de um bit por símbolo. */
  ASKModem(size_t sample_size, double fc, double bitrate, double A);

  /** Cria uma cópia polimórfica do modem ASK. */
  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<ASKModem>(*this);
  }

  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;

protected:
  /** Limiar de energia usado para decidir entre 0 e 1. */
  double threshold;
  /** Amostras da portadora usada pelo modulador ASK. */
  signal_sample carrier;
};

/** @brief Modulação FSK, alternando entre duas frequências. */
class FSKModem : public PassbandModem {
public:
  /** Configura um modem FSK de um bit por símbolo. */
  FSKModem(size_t sample_size, double fc, double bitrate, double A, double fd);

  /** Cria uma cópia polimórfica do modem FSK. */
  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<FSKModem>(*this);
  }

  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;

protected:
  /** Deslocamento aplicado à frequência da portadora. */
  double fd;
  /** Portadora associada ao bit 0, em `fc - fd`. */
  signal_sample f1_carrier;
  /** Portadora associada ao bit 1, em `fc + fd`. */
  signal_sample f2_carrier;
};

/** @brief Base QAM/PSK que representa símbolos em coordenadas I/Q. */
class QAM : public PassbandModem {
public:
  /** Configura o tamanho da constelação e suas portadoras I/Q. */
  QAM(size_t symbol_size, size_t sample_size, double fc, double bitrate,
      double A);

  void modulate(symbol &s, signal_sample &sample) const override;
  void demodulate(signal_sample &sample, symbol &s) const override;

protected:
  /** Portadoras em fase e em quadratura. */
  signal_sample I, Q;
  /** Mapeamento dos símbolos para pontos da constelação. */
  IQ_map constellation_map;
  /** Fator de normalização para componentes diagonais. */
  static constexpr double inv_sqrt2 = 1 / std::numbers::sqrt2_v<double>;

  /** Gera as amostras das portadoras I e Q. */
  void set_IQ_signals();
};

/** @brief Modulação 8-PSK com oito pontos na constelação. */
class _8PSKModem : public QAM {
public:
  /**
   * Define uma constelação de símbolos com com 8 pontos em um anel de raio A.
   *
   * Cada símbolo carrega 3 bits de fase, em código Gray (000, 001, 011, 010, 
   * 110, 111, 101 e 100), em sentido anti-horário a partir do primeiro ponto do anel.
   */
  _8PSKModem(size_t sample_size, double fc, double bitrate, double A);

  /** Cria uma cópia polimórfica do modem 8-PSK. */
  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<_8PSKModem>(*this);
  }
};

/** @brief Modulação 32-QAM com trinta e dois pontos na constelação. */
class _32QAM : public QAM {
public:
  /**
   * Define uma constelação de símbolos com 4 anéis de 8 pontos (raios 1 a 4, em
   * unidades de A).
   *
   * Cada símbolo carrega 5 bits:
   *  - 2 bits de anel, em código Gray: r = 1 -> 00, 2 -> 01, 3 -> 11, 4 -> 10;
   *  - 3 bits de fase, em código Gray (000, 001, 011, 010, 110, 111, 101, 100),
   *    em sentido anti-horário a partir do primeiro ponto do anel (similar ao
   * 8PSK). Os anéis 2 e 4 são rotacionados em 22,5° em relação aos anéis 1 e 3,
   * e o sinal tem pico de 4A.
   */
  _32QAM(size_t sample_size, double fc, double bitrate, double A);

  /** Cria uma cópia polimórfica do modem 32-QAM. */
  std::shared_ptr<Modem> clone() const override {
    return std::make_shared<_32QAM>(*this);
  }

protected:
  /** Componentes trigonométricas usadas nos pontos intermediários. */
  const double cos22 = cos(22.5 * (PI / 180));
  const double sin22 = sin(22.5 * (PI / 180));
};

} // namespace PhysicalLayer

#endif