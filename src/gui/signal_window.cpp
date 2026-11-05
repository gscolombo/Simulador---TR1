#include "gui/signal_window.hpp"
#include "modem.hpp"

using namespace Gui;
using namespace PhysicalLayer;

namespace {
constexpr int REFRESH_INTERVAL_MS = 50;
}

SignalWindow::SignalWindow(const std::array<std::string, 2> &names,
                           const Config::Modem::ModemConfig &modem,
                           const std::string &trace_path,
                           const std::array<pid_t, 2> &peer_pids)
    : reader(trace_path) {
  set_title("Simulador TR1 — Sinais da camada física");
  set_default_size(900, 720);

  hint.set_text("Roda do mouse: zoom · arrastar: deslocar · duplo clique: ver "
                "tudo. Os gráficos de um mesmo sentido são sincronizados.");
  hint.set_xalign(0);
  hint.set_wrap(true);

  // Taxa de amostragem = amostras por símbolo × baud
  const double sample_rate = (double)modem.sample_size * modem.baud();

  for (size_t direction = 0; direction < 2; direction++) {
    const std::string route =
        names[direction] + " → " + names[1 - direction] + ": ";

    add_plot(direction * 2, route + "sinal transmitido (entrada do canal)",
             peer_pids[direction], TRACE_IN, sample_rate, modem.sample_size);
    add_plot(direction * 2 + 1,
             route + "sinal recebido (saída do canal, com ruído)",
             peer_pids[direction], TRACE_OUT, sample_rate, modem.sample_size);

    // Zoom e deslocamento é aplicado no par transmitido/recebido
    SignalPlot &sent = plots[direction * 2],
               &received = plots[direction * 2 + 1];
    sent.signal_view_changed.connect([&received](double first, double count) {
      received.set_view(first, count);
    });
    received.signal_view_changed.connect(
        [&sent](double first, double count) { sent.set_view(first, count); });
  }

  scroller.set_child(content);
  scroller.set_expand(true);

  root.set_margin(8);
  root.append(hint);
  root.append(scroller);
  set_child(root);

  // Fechar a janela apenas a oculta; a captura continua
  signal_close_request().connect(
      [this] {
        set_visible(false);
        return true;
      },
      false);

  tick = Glib::signal_timeout().connect(
      sigc::mem_fun(*this, &SignalWindow::on_tick), REFRESH_INTERVAL_MS);
}

SignalWindow::~SignalWindow() { tick.disconnect(); }

void SignalWindow::add_plot(size_t index, const std::string &caption, pid_t pid,
                            TraceStage stage, double sample_rate,
                            size_t samples_per_symbol) {
  auto *label = Gtk::make_managed<Gtk::Label>(caption);
  label->set_xalign(0);
  label->set_margin_top(6);

  SignalPlot &plot = plots[index];
  plot.set_samples(&reader.signal(pid, stage).samples);
  plot.set_timing(sample_rate, samples_per_symbol);

  content.append(*label);
  content.append(plot);
}

void SignalWindow::finish() {
  tick.disconnect();
  on_tick();
}

bool SignalWindow::on_tick() {
  if (reader.poll())
    for (SignalPlot &plot : plots)
      plot.queue_draw();

  return true;
}
