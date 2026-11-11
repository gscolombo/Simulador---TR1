#ifndef LINK_WINDOW_H
#define LINK_WINDOW_H

#include "simulator.hpp"
#include <gtkmm.h>
#include <string>

namespace Gui {

class LinkWindow : public Gtk::Window {
public:
  explicit LinkWindow(const std::array<std::string, 2> &names);

  void show_event(PeerId peer, const std::string &stage,
                  const std::string &bits);
  void clear();

private:
  std::array<std::string, 2> names;
  Gtk::Box root{Gtk::Orientation::VERTICAL, 6};
  Gtk::ScrolledWindow scroller;
  Gtk::TextView history;
};

} // namespace Gui

#endif
