#ifndef CHAT_WINDOW_H
#define CHAT_WINDOW_H

#include <gtkmm.h>
#include <string>

namespace Gui {

// Janela de chat de um peer
class ChatWindow : public Gtk::Window {
public:
  // Constrói a janela de chat de um peer
  ChatWindow(const std::string &own_name, const std::string &remote_name);

  // Texto digitado pelo usuário para transmissão
  sigc::signal<void(const std::string &)> signal_send;

  // Insere mensagens recebidas pelo outro peer no chat
  void show_received(const std::string &text);

  // Insere mensagens do sistema no chat
  void show_system(const std::string &text);

  // Ativa/desativa o envio de mensagens no chat
  void set_enabled(bool enabled);

  const std::string get_peer_name() { return name; };

private:
  // Callback para envio de mensagem em resposta a sinal (clique ou Enter)
  void on_send();
  // Insere um texto no chat
  void append(const std::string &line);

  std::string name;        // Nome do peer remetente
  std::string remote_name; // Nome do peer destinatário

  Gtk::Box root{Gtk::Orientation::VERTICAL, 6};
  Gtk::ScrolledWindow scroller;
  Gtk::TextView history; // Histórico de mensagens do chat
  Gtk::Box input_row{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Entry entry; // Campo para entrada de texto
  Gtk::Button send_button{"Enviar"};
};

} // namespace Gui

#endif
