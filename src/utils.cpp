/**
 * @file utils.cpp
 * @brief Implementação das funções auxiliares da simulação.
 */

#include "utils.hpp"
#include <stdexcept>

int check_err(int err, const char *message) {
  if (err == -1)
    throw std::runtime_error(message);

  return 0;
}