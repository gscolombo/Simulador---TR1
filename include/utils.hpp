#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <sys/un.h>

int check_err(int err, const char *message);

// Conversão entre número real e texto, independente de locale (usada nos
// argumentos de linha de comando dos processos)
std::string number_to_string(double value);
double string_to_number(const std::string &text);

#endif