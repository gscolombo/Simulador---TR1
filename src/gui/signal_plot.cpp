#include "gui/signal_plot.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace Gui;

namespace {

constexpr double MIN_VISIBLE_SAMPLES = 16.;

// Tempo em ms (ou s, se o intervalo visível passar de 1 s)
std::string format_time(double seconds, bool use_seconds) {
  char text[32];
  if (use_seconds)
    std::snprintf(text, sizeof(text), "%.4g s", seconds);
  else
    std::snprintf(text, sizeof(text), "%.4g ms", seconds * 1e3);

  return text;
}

} // namespace

SignalPlot::SignalPlot() {
  set_content_height(170);
  set_hexpand(true);
  set_draw_func(sigc::mem_fun(*this, &SignalPlot::on_draw));

  auto scroll = Gtk::EventControllerScroll::create();
  scroll->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
  scroll->signal_scroll().connect(sigc::mem_fun(*this, &SignalPlot::on_scroll),
                                  true);
  add_controller(scroll);

  // Somente movimento horizontal
  auto motion = Gtk::EventControllerMotion::create();
  motion->signal_motion().connect(
      [this](double x, double) { pointer_x = x; });
  add_controller(motion);

  auto drag = Gtk::GestureDrag::create();
  drag->signal_drag_begin().connect(
      [this](double, double) { drag_first = visible_first(); });
  drag->signal_drag_update().connect(
      sigc::mem_fun(*this, &SignalPlot::on_drag_update));
  add_controller(drag);

  auto click = Gtk::GestureClick::create();
  click->signal_pressed().connect([this](int n_press, double, double) {
    if (n_press == 2)
      apply_view(0., 0., true);
  });
  add_controller(click);
}

void SignalPlot::set_timing(double sample_rate, size_t samples_per_symbol) {
  this->sample_rate = sample_rate;
  this->symbol_samples = (double)samples_per_symbol;
  queue_draw();
}

void SignalPlot::set_view(double first, double count) {
  apply_view(first, count, false);
}

void SignalPlot::apply_view(double first, double count, bool notify) {
  const double n = samples ? (double)samples->size() : 0.;

  // Janela cobrindo (quase) todo o sinal equivale a "ver tudo"
  if (count <= 0. || count >= n * 0.999) {
    view_first = 0.;
    view_count = 0.;
  } else {
    view_count = std::max(count, std::min(MIN_VISIBLE_SAMPLES, n));
    view_first = std::clamp(first, 0., n - view_count);
  }

  queue_draw();
  if (notify)
    signal_view_changed.emit(view_first, view_count);
}

double SignalPlot::visible_count() const {
  const double n = samples ? (double)samples->size() : 0.;
  return view_count > 0. ? std::min(view_count, n) : n;
}

double SignalPlot::visible_first() const {
  if (view_count <= 0.)
    return 0.;

  const double n = samples ? (double)samples->size() : 0.;
  return std::clamp(view_first, 0., std::max(0., n - visible_count()));
}

bool SignalPlot::on_scroll(double, double dy) {
  if (!samples || samples->size() < 2 || plot_width() < 2)
    return false;

  const double count = visible_count(), first = visible_first();
  const double fraction = std::clamp((pointer_x - LEFT) / plot_width(), 0., 1.);
  const double anchor = first + fraction * count; // Amostra sob o ponteiro

  const double new_count = count * (dy < 0 ? 0.8 : 1.25);
  apply_view(anchor - fraction * new_count, new_count, true);
  return true;
}

void SignalPlot::on_drag_update(double offset_x, double) {
  if (!samples || plot_width() < 2 || view_count <= 0.)
    return;

  const double count = visible_count();
  apply_view(drag_first - offset_x * count / plot_width(), count, true);
}

void SignalPlot::on_draw(const Cairo::RefPtr<Cairo::Context> &cr, int width,
                         int height) {
  const double pw = width - LEFT - RIGHT, ph = height - TOP - BOTTOM;
  const size_t n = samples ? samples->size() : 0;

  cr->set_source_rgb(1., 1., 1.);
  cr->paint();
  cr->set_source_rgb(0.97, 0.97, 0.97);
  cr->rectangle(LEFT, TOP, pw, ph);
  cr->fill();

  cr->set_font_size(11);
  if (n == 0 || pw < 2 || ph < 2) {
    cr->set_source_rgb(0.5, 0.5, 0.5);
    cr->move_to(LEFT + 12, TOP + ph / 2);
    cr->show_text("Sem sinal");
    return;
  }

  // Nova transmissão (o vetor encolheu): volta a exibir o sinal inteiro
  if (n < last_size) {
    view_first = 0.;
    view_count = 0.;
  }
  last_size = n;

  const double count = visible_count(), first = visible_first();
  const double per_pixel = count / pw; // Amostras por pixel

  // Mínimo e máximo de cada coluna (ou as próprias amostras, se esparsas)
  std::vector<double> lo, hi;
  double ymin = std::numeric_limits<double>::infinity(), ymax = -ymin;

  if (per_pixel > 1.) {
    lo.assign((size_t)pw, 0.);
    hi.assign((size_t)pw, 0.);
    for (size_t x = 0; x < lo.size(); x++) {
      size_t i0 = (size_t)(first + x * per_pixel);
      size_t i1 = std::min(n, (size_t)(first + (x + 1) * per_pixel) + 1);
      auto [mn, mx] = std::minmax_element(samples->begin() + i0,
                                          samples->begin() + std::max(i0 + 1, i1));
      lo[x] = *mn;
      hi[x] = *mx;
      ymin = std::min(ymin, *mn);
      ymax = std::max(ymax, *mx);
    }
  } else {
    size_t i0 = (size_t)first, i1 = std::min(n, (size_t)std::ceil(first + count) + 1);
    for (size_t i = i0; i < i1; i++) {
      ymin = std::min(ymin, (*samples)[i]);
      ymax = std::max(ymax, (*samples)[i]);
    }
  }

  if (ymax - ymin < 1e-12) {
    ymin -= 1.;
    ymax += 1.;
  }
  const double margin = (ymax - ymin) * 0.05;
  ymin -= margin;
  ymax += margin;

  auto y_of = [&](double v) { return TOP + ph * (1. - (v - ymin) / (ymax - ymin)); };

  // Grade e rótulos dos eixos
  const bool use_seconds = count / sample_rate >= 1.;
  cr->set_line_width(1.);
  for (int k = 0; k <= 4; k++) {
    const double fraction = k / 4.;
    const double gx = LEFT + fraction * pw, gy = TOP + fraction * ph;

    cr->set_source_rgb(0.85, 0.85, 0.85);
    cr->move_to(gx, TOP);
    cr->line_to(gx, TOP + ph);
    cr->move_to(LEFT, gy);
    cr->line_to(LEFT + pw, gy);
    cr->stroke();

    cr->set_source_rgb(0.25, 0.25, 0.25);
    std::string time =
        format_time((first + fraction * count) / sample_rate, use_seconds);
    Cairo::TextExtents extents;
    cr->get_text_extents(time, extents);
    cr->move_to(std::clamp(gx - extents.width / 2, 0., (double)width - extents.width),
                height - 6);
    cr->show_text(time);

    // Evita rótulos como "5e-15 V" no zero (erro de arredondamento)
    double level = ymax - fraction * (ymax - ymin);
    if (std::fabs(level) < (ymax - ymin) * 1e-9)
      level = 0.;

    char volts[32];
    std::snprintf(volts, sizeof(volts), "%.4g V", level);
    cr->get_text_extents(volts, extents);
    cr->move_to(LEFT - 6 - extents.width, gy + 4);
    cr->show_text(volts);
  }

  cr->save();
  cr->rectangle(LEFT, TOP, pw, ph);
  cr->clip();

  // Limites de símbolo (quando houver espaço para distingui-los)
  const double symbol_px = symbol_samples * pw / count;
  if (symbol_samples > 0. && symbol_px >= 8.) {
    cr->set_source_rgba(0.2, 0.5, 0.9, 0.35);
    cr->set_dash(std::vector<double>{3., 3.}, 0.);
    for (double s = std::ceil(first / symbol_samples) * symbol_samples;
         s < first + count; s += symbol_samples) {
      const double sx = LEFT + (s - first) * pw / count;
      cr->move_to(sx, TOP);
      cr->line_to(sx, TOP + ph);
    }
    cr->stroke();
    cr->unset_dash();
  }

  // Forma de onda
  cr->set_source_rgb(0.1, 0.3, 0.8);
  if (per_pixel > 1.) {
    for (size_t x = 0; x < lo.size(); x++) {
      cr->move_to(LEFT + x + 0.5, y_of(lo[x]));
      cr->line_to(LEFT + x + 0.5, y_of(hi[x]) - 0.5);
    }
  } else {
    const size_t i0 = (size_t)first, i1 = std::min(n, (size_t)std::ceil(first + count) + 1);
    for (size_t i = i0; i < i1; i++) {
      const double px = LEFT + ((double)i - first) * pw / count;
      if (i == i0)
        cr->move_to(px, y_of((*samples)[i]));
      else
        cr->line_to(px, y_of((*samples)[i]));
    }
  }
  cr->stroke();
  cr->restore();
}
