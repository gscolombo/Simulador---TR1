#include "gui/link_window.hpp"

using namespace Gui;

LinkWindow::LinkWindow(const std::array<std::string, 2> &names) : names(names) {
  set_title("Simulador TR1 - Camada de enlace");
  set_default_size(900, 600);

  history.set_editable(false);
  history.set_cursor_visible(false);
  history.set_monospace(true);
  history.set_wrap_mode(Gtk::WrapMode::CHAR);
  scroller.set_child(history);
  scroller.set_expand(true);

  root.set_margin(8);
  root.append(scroller);
  set_child(root);

  signal_close_request().connect(
      [this] {
        set_visible(false);
        return true;
      },
      false);
}

void LinkWindow::show_event(PeerId peer, const std::string &stage,
                            const std::string &bits) {
  const std::string direction = stage.rfind("-tx") != std::string::npos
                                    ? names[peer] + " -> " + names[1 - peer]
                                    : names[1 - peer] + " -> " + names[peer];
  const std::string line = direction + " | " + stage + " | " + bits + "\n";
  auto buffer = history.get_buffer();
  buffer->insert(buffer->end(), line);
  auto mark = buffer->create_mark(buffer->end());
  history.scroll_to(mark);
  buffer->delete_mark(mark);
}

void LinkWindow::clear() { history.get_buffer()->set_text(""); }
