/**
 * @file utils.hpp
 * @brief Funções auxiliares da simulação.
 */
#ifndef UTILS_HPP
#define UTILS_HPP

#include <sys/un.h>
/**
 * @brief Verifica o resultado de uma chamada de sistema.
 * @param err Valor retornado pela operação verificada.
 * @param message Mensagem usada na exceção quando a operação falha.
 * @return Zero quando `err` não indica falha.
 * @throws std::runtime_error Quando `err` é igual a `-1`.
 */
int check_err(int err, const char *message);

#endif