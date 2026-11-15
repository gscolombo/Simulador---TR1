#include "channel.hpp"
#include "utils.hpp"
#include <cstring>
#include <errno.h>
#include <iostream>
#include <unistd.h>

using namespace PhysicalLayer;

int main(int argc, char **argv) {
  try {
    double mu, sigma;

    if (argc < 3) {
      std::cerr << "Argumentos inválidos" << std::endl
                << "Uso: channel [média desvio_padrão [arquivo_de_sinal]]"
                << std::endl;
      return 1;
    } else {
      mu = string_to_number(argv[1]);
      sigma = string_to_number(argv[2]);
    }

    Channel channel(mu, sigma);
    if (argc == 4)
      channel.set_trace_file(argv[3]);

    channel.open();
    channel.stream();
    channel.close();

  } catch (const std::exception &e) {
    std::cerr << e.what() << " (" << strerror(errno) << ")" << std::endl;
    return 1;
  }

  return 0;
}