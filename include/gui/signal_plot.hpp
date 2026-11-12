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
  // Registra os callbacks para interação do gráfico com o mouse
  SignalPlot();

  // Amostras exibidas (o vetor deve continuar válido enquanto o gráfico
  // existir)
  void set_samples(const std::vector<double> *samples) {
    this->samples = samples;
  }

  // Setter da taxa de amostragem e número de amostras por símbolo
  void set_timing(double sample_rate, size_t samples_per_symbol);

  // Define a janela visível (count = 0: sinal inteiro), sem notificar
  void set_view(double first, double count);

  // Emitido quando o usuário altera a janela visível (permite sincronizar)
  sigc::signal<void(double, double)> signal_view_changed;

private:
  // Margens do gráfico
  static constexpr int LEFT = 64, RIGHT = 10, TOP = 8, BOTTOM = 22;

  void on_draw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height);

  /**
    @brief Callback para atualizar amostras visíveis em resposta à
    rolagem do mouse
    @param dx Rolamento no eixo X (não é utilizado)
    @param dy Rolamento no eixo Y
  */
  bool on_scroll(double dx, double dy);

  /**
    @brief Callback para atualizar amostras visíveis em resposta ao arrasto do
    gráfico no eixo X
    @param offset_x Deslocamento do ponteiro no eixo X
    @param offset_y Deslocamento do ponteiro no eixo Y (não é utilizado)
  */
  void on_drag_update(double offset_x, double offset_y);

  /**
    @brief Atualiza o número da primeira amostra visível e o número de amostras
    visíveis e redesenha o gráfico
    @param first Número da primeira amostra visível (entre 0 e `sample.size() -
    view_count`, inclusivo)
    @param count Número de amostras visíveis
    @param notify Emite o sinal para atualizar o gráfico, se verdadeiro
    */
  void apply_view(double first, double count, bool notify);

  // Retorna o número de amostras visíveis
  double visible_count() const;

  // Retorna o índice da primeira amostra visível, limitado para a janela não
  // passar do fim do sinal (0 se tudo visível).
  double visible_first() const;

  double plot_width() const { return get_width() - LEFT - RIGHT; }

  const std::vector<double> *samples = nullptr;
  double sample_rate = 1.;
  double symbol_samples = 0.;

  double view_first = 0.; // Primeira amostra visível
  double view_count =
      0.; // Número de amostras visíveis (se 0, todo o sinal é exibido)
  size_t last_size = 0;
  double pointer_x = 0., drag_first = 0.;
};

} // namespace Gui

#endif
