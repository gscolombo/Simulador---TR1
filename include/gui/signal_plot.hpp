#ifndef SIGNAL_PLOT_H
#define SIGNAL_PLOT_H

#include <gtkmm.h>
#include <vector>

namespace Gui {

/*
  Gráfico de um sinal no tempo (tensão em V). 
  Roda do mouse: zoom; 
  arrastar: deslocar; 
  duplo clique: ver tudo. 
  Com muitas amostras por pixel, desenha o mínimo e o máximo de cada coluna.
*/
class SignalPlot : public Gtk::DrawingArea {
public:
  SignalPlot();

  // Amostras exibidas (o vetor deve continuar válido enquanto o gráfico existir)
  void set_samples(const std::vector<double> *samples) { this->samples = samples; }
  void set_timing(double sample_rate, size_t samples_per_symbol);

  // Define a janela visível (count = 0: sinal inteiro), sem notificar
  void set_view(double first, double count);

  // Emitido quando o usuário altera a janela visível (permite sincronizar)
  sigc::signal<void(double, double)> signal_view_changed;

private:
  static constexpr int LEFT = 64, RIGHT = 10, TOP = 8, BOTTOM = 22;

  void on_draw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height);
  bool on_scroll(double dx, double dy);
  void on_drag_update(double offset_x, double offset_y);
  void apply_view(double first, double count, bool notify);
  double visible_count() const;
  double visible_first() const;
  double plot_width() const { return get_width() - LEFT - RIGHT; }

  const std::vector<double> *samples = nullptr;
  double sample_rate = 1.;
  double symbol_samples = 0.;

  double view_first = 0., view_count = 0.; // view_count = 0: sinal inteiro
  size_t last_size = 0;
  double pointer_x = 0., drag_first = 0.;
};

} // namespace Gui

#endif
