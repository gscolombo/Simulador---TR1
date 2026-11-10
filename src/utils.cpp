/**
 * @file utils.cpp
 * @brief Implementação das funções auxiliares da simulação.
 */

#include "utils.hpp"
#include <charconv>
#include <stdexcept>

int check_err(int err, const char *message) {
  if (err == -1)
    throw std::runtime_error(message);

  return 0;
}

std::string number_to_string(double value) {
  char buf[64];
  auto result = std::to_chars(buf, buf + sizeof(buf), value);
  return std::string(buf, result.ptr);
}

double string_to_number(const std::string &text) {
  double value = 0.;
  const char *end = text.data() + text.size();
  auto result = std::from_chars(text.data(), end, value);

  if (result.ec != std::errc{} || result.ptr != end)
    throw std::invalid_argument("Número inválido: '" + text + "'");

  return value;
}
