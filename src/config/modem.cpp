#include "config/modem.hpp"
#include "utils.hpp"

#include <cmath>
#include <locale>
#include <sstream>
#include <stdexcept>

using namespace Config::Modem;
using namespace PhysicalLayer;

namespace {

struct TypeInfo {
  ModemType type;
  const char *key;   // Identificador usado nos argumentos de linha de comando
  const char *label; // Nome exibido ao usuário
  bool passband;
  size_t bits_per_symbol;
};

const std::vector<TypeInfo> &type_table() {
  static const std::vector<TypeInfo> table = {
      {ModemType::NRZPolar, "nrz", "NRZ-Polar", false, 1},
      {ModemType::Manchester, "manchester", "Manchester", false, 1},
      {ModemType::Bipolar, "bipolar", "Bipolar", false, 1},
      {ModemType::ASK, "ask", "ASK", true, 1},
      {ModemType::FSK, "fsk", "FSK", true, 1},
      {ModemType::PSK8, "8psk", "8PSK", true, 3},
      {ModemType::QAM32, "32qam", "32-QAM", true, 5},
  };
  return table;
}

const TypeInfo &info(ModemType type) {
  for (const TypeInfo &i : type_table())
    if (i.type == type)
      return i;

  throw std::invalid_argument("Tipo de modem desconhecido.");
}

// Formata número para mensagens de erro (sem depender do locale)
std::string fmt(double value) {
  std::ostringstream os;
  os.imbue(std::locale::classic());
  os << value;
  return os.str();
}

size_t to_size(const std::string &text) {
  double value = string_to_number(text);
  if (value < 0. || value != std::floor(value))
    throw std::invalid_argument("Valor inteiro inválido: '" + text + "'");

  return (size_t)value;
}

} // namespace

/* Getters */
size_t ModemConfig::bits_per_symbol() const {
  return info(type).bits_per_symbol;
}

const std::vector<ModemType> &Config::Modem::all_modem_types() {
  static const std::vector<ModemType> types = [] {
    std::vector<ModemType> v;
    for (const TypeInfo &i : type_table())
      v.push_back(i.type);
    return v;
  }();
  return types;
}

const char *Config::Modem::modem_label(ModemType type) {
  return info(type).label;
}

bool Config::Modem::is_passband(ModemType type) { return info(type).passband; }

/* Valida a configuração do modem*/
std::string Config::Modem::validate(const ModemConfig &c) {
  if (c.sample_size == 0)
    return "As amostras por símbolo devem ser maiores que zero.";
  if (!(c.bitrate > 0.))
    return "A taxa de bits deve ser maior que zero.";
  if (!(c.amplitude > 0.))
    return "A amplitude deve ser maior que zero.";

  if (c.type == ModemType::Manchester && c.sample_size % 2 != 0)
    return "O Manchester exige um número par de amostras por símbolo.";

  if (!is_passband(c.type))
    return "";

  // As mesmas condições verificadas (por assert) no construtor de PassbandModem
  if (!(c.carrier_freq > 0.))
    return "A frequência da portadora deve ser maior que zero.";

  double baud = c.baud();
  if (!(c.carrier_freq >= baud * 10.))
    return "A portadora deve ser ao menos 10 vezes o baud (" + fmt(baud) +
           " Bd).";
  if (!(c.sample_size > 2 * (c.carrier_freq / baud)))
    return "Amostras por símbolo insuficientes para a portadora.";

  if (c.type == ModemType::FSK) {
    if (!(c.freq_deviation > 0.) || !(c.freq_deviation < c.carrier_freq))
      return "O desvio de frequência deve estar entre 0 e a portadora.";
    if (!(c.sample_size > 2 * ((c.carrier_freq + c.freq_deviation) / baud)))
      return "Amostras por símbolo insuficientes para fc + fd.";
  }

  return "";
}

std::shared_ptr<Modem> Config::Modem::make_modem(const ModemConfig &c) {
  std::string error = validate(c);
  if (!error.empty())
    throw std::invalid_argument(error);

  switch (c.type) {
  case ModemType::NRZPolar:
    return std::make_shared<NRZPolarModem>(c.sample_size, c.bitrate,
                                           c.amplitude);
  case ModemType::Manchester:
    return std::make_shared<ManchesterModem>(c.sample_size, c.bitrate,
                                             c.amplitude);
  case ModemType::Bipolar:
    return std::make_shared<BipolarModem>(c.sample_size, c.bitrate,
                                          c.amplitude);
  case ModemType::ASK:
    return std::make_shared<ASKModem>(c.sample_size, c.carrier_freq, c.bitrate,
                                      c.amplitude);
  case ModemType::FSK:
    return std::make_shared<FSKModem>(c.sample_size, c.carrier_freq, c.bitrate,
                                      c.amplitude, c.freq_deviation);
  case ModemType::PSK8:
    return std::make_shared<_8PSKModem>(c.sample_size, c.carrier_freq,
                                        c.bitrate, c.amplitude);
  case ModemType::QAM32:
    return std::make_shared<_32QAM>(c.sample_size, c.carrier_freq, c.bitrate,
                                    c.amplitude);
  }

  throw std::invalid_argument("Tipo de modem desconhecido.");
}

std::vector<std::string> Config::Modem::to_args(const ModemConfig &c) {
  return {std::string("modem=") + info(c.type).key,
          "sample_size=" + std::to_string(c.sample_size),
          "fc=" + number_to_string(c.carrier_freq),
          "bitrate=" + number_to_string(c.bitrate),
          "amplitude=" + number_to_string(c.amplitude),
          "fd=" + number_to_string(c.freq_deviation)};
}

ModemConfig Config::Modem::parse_args(const std::vector<std::string> &args) {
  ModemConfig config;

  for (const std::string &arg : args) {
    size_t eq = arg.find('=');
    if (eq == std::string::npos)
      throw std::invalid_argument("Argumento inválido: '" + arg + "'");

    std::string key = arg.substr(0, eq), value = arg.substr(eq + 1);

    if (key == "modem") {
      bool found = false;
      for (const TypeInfo &i : type_table())
        if (value == i.key) {
          config.type = i.type;
          found = true;
        }
      if (!found)
        throw std::invalid_argument("Modem desconhecido: '" + value + "'");
    } else if (key == "sample_size")
      config.sample_size = to_size(value);
    else if (key == "fc")
      config.carrier_freq = string_to_number(value);
    else if (key == "bitrate")
      config.bitrate = string_to_number(value);
    else if (key == "amplitude")
      config.amplitude = string_to_number(value);
    else if (key == "fd")
      config.freq_deviation = string_to_number(value);
    else
      throw std::invalid_argument("Parâmetro desconhecido: '" + key + "'");
  }

  return config;
}
