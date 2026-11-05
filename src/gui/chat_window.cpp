#include "gui/chat_window.hpp"

using namespace Gui;

namespace {

// Mensagens corrompidas pelo canal podem não ser UTF-8 válido
Glib::ustring to_utf8(const std::string &text) {
  gchar *valid = g_utf8_make_valid(text.c_str(), text.size());
  Glib::ustring result(valid);
  g_free(valid);

  return result;
}

} // namespace

ChatWindow::ChatWindow(const std::string &own_name,
                       const std::string &remote_name)
    : remote_name(remote_name) {
  set_title("Chat — " + own_name);
  set_default_size(420, 480);

  history.set_editable(false);
  history.set_cursor_visible(false);
  history.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);

  scroller.set_child(history);
  scroller.set_expand(true);

  entry.set_hexpand(true);
  input_row.append(entry);
  input_row.append(send_button);

  root.set_margin(8);
  root.append(scroller);
  root.append(input_row);
  set_child(root);

  entry.signal_activate().connect(sigc::mem_fun(*this, &ChatWindow::on_send));
  send_button.signal_clicked().connect(
      sigc::mem_fun(*this, &ChatWindow::on_send));

  // Fechar a janela apenas a oculta; a simulação continua em execução
  signal_close_request().connect(
      [this] {
        set_visible(false);
        return true;
      },
      false);
}

void ChatWindow::on_send() {
  const std::string text = entry.get_text();
  if (text.empty())
    return;

  if (text.size() > MAX_MESSAGE_BYTES) {
    show_system("Mensagem muito longa (" + std::to_string(text.size()) +
                " bytes; máximo de " + std::to_string(MAX_MESSAGE_BYTES) +
                ").");
    return;
  }

  append("Você: " + text);
  signal_send.emit(text);
  entry.set_text("");
}

void ChatWindow::show_received(const std::string &text) {
  // Avisos de entrada/saída do outro peer são mensagens do próprio protocolo
  if (text == remote_name + " entrou" || text == remote_name + " saiu")
    show_system(text);
  else
    append(remote_name + ": " + text);
}

void ChatWindow::show_system(const std::string &text) {
  append("— " + text);
}

void ChatWindow::set_enabled(bool enabled) {
  entry.set_sensitive(enabled);
  send_button.set_sensitive(enabled);
}

void ChatWindow::append(const std::string &line) {
  auto buffer = history.get_buffer();
  buffer->insert(buffer->end(), to_utf8(line) + "\n");

  // Rola o histórico até a última mensagem
  auto mark = buffer->create_mark(buffer->end());
  history.scroll_to(mark);
  buffer->delete_mark(mark);
}
