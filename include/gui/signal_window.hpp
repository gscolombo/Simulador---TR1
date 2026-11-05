#ifndef SIGNAL_WINDOW_H
#define SIGNAL_WINDOW_H

#include "config/modem.hpp"
#include "signal_plot.hpp"
#include "trace_reader.hpp"
#include <array>
#include <gtkmm.h>
#include <string>
#include <sys/types.h>

namespace Gui {

/*
  Visualizador de sinais da camada física. Para cada sentido da comunicação
  mostra o sinal transmitido (entrada do canal) e o sinal recebido (saída do
  canal, já com ruído), lidos do arquivo de registro do sinal gravado pelo
  canal.
*/
class SignalWindow : public Gtk::Window {
public:
  SignalWindow(const std::array<std::string, 2> &names,
               const Config::Modem::ModemConfig &modem,
               const std::string &trace_path,
               const std::array<pid_t, 2> &peer_pids);
  ~SignalWindow() override;

  // Faz a última leitura do traço e interrompe a atualização periódica
  void finish();

private:
  void add_plot(size_t index, const std::string &caption, pid_t pid,
                PhysicalLayer::TraceStage stage, double sample_rate,
                size_t samples_per_symbol);
  bool on_tick();

  TraceReader reader;
  Gtk::Box root{Gtk::Orientation::VERTICAL, 6};
  Gtk::Label hint;
  Gtk::ScrolledWindow scroller;
  Gtk::Box content{Gtk::Orientation::VERTICAL, 4};
  std::array<SignalPlot, 4> plots; // [sentido * 2 + etapa]
  sigc::connection tick;
};

} // namespace Gui

#endif
