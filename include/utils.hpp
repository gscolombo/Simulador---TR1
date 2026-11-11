/**
 * @file utils.hpp
 * @brief Funções auxiliares da simulação.
 */
#ifndef UTILS_HPP
#define UTILS_HPP

#include "types.hpp"
#include <string>
#include <sys/un.h>
/**
 * @brief Verifica o resultado de uma chamada de sistema.
 * @param err Valor retornado pela operação verificada.
 * @param message Mensagem usada na exceção quando a operação falha.
 * @return Zero quando `err` não indica falha.
 * @throws std::runtime_error Quando `err` é igual a `-1`.
 */

int check_err(int err, const char *message);

// Conversão entre número real e texto, independente de locale (usada nos
// argumentos de linha de comando dos processos)
std::string number_to_string(double value);
double string_to_number(const std::string &text);

/**
 * @brief Função auxiliar para enviar um vetor de bytes completo a um socket
 * @param fd Descritor do arquivo de escrita
 * @param data Vetor de bytes para envio
 */
void _send(int fd, const PhysicalLayer::bytes &data);

#endif