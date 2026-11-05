#ifndef CHAT_WINDOW_H
#define CHAT_WINDOW_H

#include <gtkmm.h>
#include <string>

namespace Gui {

// Janela de chat de um peer
class ChatWindow : public Gtk::Window {
public:
  // Limite de bytes por mensagem (o peer descarta payloads maiores que 255 B,
  // incluindo o delimitador \0)
  static constexpr size_t MAX_MESSAGE_BYTES = 254;

  ChatWindow(const std::string &own_name, const std::string &remote_name);

  // Texto digitado pelo usuário para transmissão
  sigc::signal<void(const std::string &)> signal_send;

  void show_received(const std::string &text);
  void show_system(const std::string &text);
  void set_enabled(bool enabled);

private:
  void on_send();
  void append(const std::string &line);

  std::string remote_name;

  Gtk::Box root{Gtk::Orientation::VERTICAL, 6};
  Gtk::ScrolledWindow scroller;
  Gtk::TextView history;
  Gtk::Box input_row{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Entry entry;
  Gtk::Button send_button{"Enviar"};
};

} // namespace Gui

#endif
