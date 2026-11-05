#ifndef MODEM_CONFIG_H
#define MODEM_CONFIG_H

#include "../modem.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Config {

namespace Modem {
enum class ModemType { NRZPolar, Manchester, Bipolar, ASK, FSK, PSK8, QAM32 };

/*
  Parâmetros de configuração de um modem.
*/
struct ModemConfig {
  ModemType type = ModemType::NRZPolar;
  size_t sample_size = 100;    // Amostras por símbolo
  double carrier_freq = 10e3;  // Hz (somente banda passante)
  double bitrate = 1e3;        // bits/s
  double amplitude = 3.3;      // V
  double freq_deviation = 1e3; // Hz (somente FSK)

  size_t bits_per_symbol() const;
  double baud() const { return bitrate / (double)bits_per_symbol(); }
};

const std::vector<ModemType> &all_modem_types();
const char *modem_label(ModemType type);
bool is_passband(ModemType type);

/*
  Verifica as restrições dos construtores dos modems (que abortam o processo
  por `assert`). Retorna uma mensagem de erro, ou string vazia se válido.
*/
std::string validate(const ModemConfig &config);

// Lança std::invalid_argument se a configuração for inválida
std::shared_ptr<PhysicalLayer::Modem> make_modem(const ModemConfig &config);

// Conversão de/para argumentos de linha de comando ("chave=valor")
std::vector<std::string> to_args(const ModemConfig &config);
ModemConfig parse_args(const std::vector<std::string> &args);

} // namespace Modem
} // namespace Config

// namespace Config

#endif
