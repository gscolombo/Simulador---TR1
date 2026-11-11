#include "gui/config_window.hpp"
#include <csignal>
#include <gtkmm.h>

int main(int argc, char **argv) {
  // Escrever em um peer que já terminou não deve encerrar a interface
  std::signal(SIGPIPE, SIG_IGN);

  auto app = Gtk::Application::create("br.unb.tr1.simulador");

  return app->make_window_and_run<Gui::ConfigWindow>(argc, argv);
}
