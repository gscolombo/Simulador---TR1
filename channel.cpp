#include "channel.hpp"
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using namespace PhysicalLayer;

int main() {
  try {
    Channel channel(0., 5e-2);
    std::cout << "Canal iniciado" << std::endl;

    channel.open();
    std::cout << "Pares conectados" << std::endl;

    channel.stream();
    channel.close();
    std::cout << "Canal fechado" << std::endl;
    sleep(2);
  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << std::endl;
    sleep(10);
  }

  return 0;
}