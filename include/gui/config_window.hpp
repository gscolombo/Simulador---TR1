#ifndef CONFIG_WINDOW_H
#define CONFIG_WINDOW_H

#include "chat_window.hpp"
#include "link_window.hpp"
#include "signal_window.hpp"
#include "simulator.hpp"
#include <array>
#include <gtkmm.h>
#include <memory>
#include <vector>

namespace Gui {

/*
  Janela principal: configura o simulador (nomes, modulação, parâmetros do
  modem e ruído) e controla sua execução. Cada peer tem uma janela de chat.
*/
class ConfigWindow : public Gtk::Window {
public:
  ConfigWindow();

private:
  SimulatorConfig read_config() const;
  std::string check(const SimulatorConfig &config) const;
  void update_form();

  void on_start();
  void on_stop();
  void on_message(PeerId peer, const std::string &text);
  void on_link_event(PeerId peer, const std::string &stage,
                     const std::string &bits);
  void on_peer_exited(PeerId peer);
  void on_finished();
  void on_failed(const std::string &reason);

  void create_chats(const SimulatorConfig &config);
  void destroy_chats();
  void show_chats();
  void show_signals();
  void show_link();
  void set_status(const std::string &text);
  void set_running(bool running);

  void add_row(int row, const char *label, Gtk::Widget &widget);
  static void setup_spin(Gtk::SpinButton &spin, double value, double min,
                         double max, double step, int digits);

  Simulator simulator;
  std::array<std::unique_ptr<ChatWindow>, 2> chats;
  std::unique_ptr<SignalWindow> signals;
  std::unique_ptr<LinkWindow> link_view;
  std::string trace_path; // Traço do sinal gravado pelo canal

  Gtk::Box root{Gtk::Orientation::VERTICAL, 10};
  Gtk::Grid form;
  Gtk::Entry name_a, name_b;
  Gtk::DropDown modulation, framing, check_type;
  Gtk::SpinButton sample_size_spin, bitrate_spin, amplitude_spin, carrier_spin,
      deviation_spin, mu_spin, sigma_spin, max_frame_spin;
  std::vector<Gtk::SpinButton *> spins;

  Gtk::Label summary_label, status_label;
  Gtk::Box buttons{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Button start_button{"Iniciar"}, stop_button{"Parar"},
      chats_button{"Mostrar chats"}, signals_button{"Mostrar sinais"},
      link_button{"Mostrar enlace"};
};

} // namespace Gui

#endif
