#include "gui/config_window.hpp"
#include "link_layer.hpp"
#include "modem.hpp"

#include <cstdio>
#include <filesystem>
#include <system_error>
#include <unistd.h>

using namespace Gui;
using namespace PhysicalLayer::Config;

ConfigWindow::ConfigWindow() {
  set_title("Simulador TR1 — Configuração");
  set_resizable(false);

  trace_path = "/tmp/tr1-sinais-" + std::to_string(getpid()) + ".bin";

  // Valores iniciais: mesmos parâmetros usados antes da interface gráfica
  const SimulatorConfig defaults;

  name_a.set_text(defaults.names[0]);
  name_b.set_text(defaults.names[1]);
  name_a.set_max_length(32);
  name_b.set_max_length(32);

  auto labels = Gtk::StringList::create({});
  for (ModemType type : all_modem_types())
    labels->append(modem_label(type));
  modulation.set_model(labels);
  for (size_t i = 0; i < all_modem_types().size(); i++)
    if (all_modem_types()[i] == defaults.modem.type)
      modulation.set_selected(i);

  framing.set_model(Gtk::StringList::create({"Contagem de caracteres",
                                             "Entrelaçamento de bits",
                                             "Byte stuffing", "Bit stuffing"}));
  framing.set_selected(2);
  check_type.set_model(Gtk::StringList::create(
      {"Nenhum", "Paridade par", "Checksum", "CRC-32", "Hamming"}));
  check_type.set_selected(3);

  setup_spin(sample_size_spin, defaults.modem.sample_size, 1, 1e6, 100, 0);
  setup_spin(bitrate_spin, defaults.modem.bitrate, 1, 1e7, 100, 0);
  setup_spin(amplitude_spin, defaults.modem.amplitude, 0.01, 1000, 0.5, 2);
  setup_spin(carrier_spin, defaults.modem.carrier_freq, 1, 1e9, 1000, 0);
  setup_spin(deviation_spin, defaults.modem.freq_deviation, 0, 1e9, 100, 0);
  setup_spin(mu_spin, defaults.noise_mu, -1000, 1000, 1, 2);
  setup_spin(sigma_spin, defaults.noise_sigma, 0, 1000, 0.5, 2);
  setup_spin(max_frame_spin, defaults.link.max_frame_size, 1, 65535, 1, 0);
  spins = {&sample_size_spin, &bitrate_spin, &amplitude_spin, &carrier_spin,
           &deviation_spin,   &mu_spin,      &sigma_spin,     &max_frame_spin};

  form.set_row_spacing(6);
  form.set_column_spacing(12);
  add_row(0, "Nome do peer A", name_a);
  add_row(1, "Nome do peer B", name_b);
  add_row(2, "Modulação", modulation);
  add_row(3, "Amostras por símbolo", sample_size_spin);
  add_row(4, "Taxa de bits (bit/s)", bitrate_spin);
  add_row(5, "Amplitude (V)", amplitude_spin);
  add_row(6, "Frequência da portadora (Hz)", carrier_spin);
  add_row(7, "Desvio de frequência FSK (Hz)", deviation_spin);
  add_row(8, "Ruído: média μ (V)", mu_spin);
  add_row(9, "Ruído: desvio padrão σ (V)", sigma_spin);
  add_row(10, "Enquadramento", framing);
  add_row(11, "Detecção/correção", check_type);
  add_row(12, "Tamanho máximo do quadro (bytes)", max_frame_spin);

  summary_label.set_xalign(0);
  summary_label.set_wrap(true);
  summary_label.set_max_width_chars(48);

  status_label.set_xalign(0);
  status_label.set_wrap(true);
  status_label.set_selectable(true);
  status_label.set_max_width_chars(48);

  buttons.append(start_button);
  buttons.append(stop_button);
  buttons.append(chats_button);
  buttons.append(signals_button);
  buttons.append(link_button);

  root.set_margin(12);
  root.append(form);
  root.append(summary_label);
  root.append(buttons);
  root.append(status_label);
  set_child(root);

  /* Registro de callbacks */

  // Reavalia o formulário a cada alteração
  modulation.property_selected().signal_changed().connect(
      sigc::mem_fun(*this, &ConfigWindow::update_form));
  name_a.signal_changed().connect(
      sigc::mem_fun(*this, &ConfigWindow::update_form));
  name_b.signal_changed().connect(
      sigc::mem_fun(*this, &ConfigWindow::update_form));
  for (Gtk::SpinButton *spin : spins)
    spin->signal_value_changed().connect(
        sigc::mem_fun(*this, &ConfigWindow::update_form));

  start_button.signal_clicked().connect(
      sigc::mem_fun(*this, &ConfigWindow::on_start));
  stop_button.signal_clicked().connect(
      sigc::mem_fun(*this, &ConfigWindow::on_stop));
  chats_button.signal_clicked().connect(
      sigc::mem_fun(*this, &ConfigWindow::show_chats));
  signals_button.signal_clicked().connect(
      sigc::mem_fun(*this, &ConfigWindow::show_signals));
  link_button.signal_clicked().connect(
      sigc::mem_fun(*this, &ConfigWindow::show_link));

  simulator.signal_message.connect(
      sigc::mem_fun(*this, &ConfigWindow::on_message));
  simulator.signal_link_event.connect(
      sigc::mem_fun(*this, &ConfigWindow::on_link_event));
  simulator.signal_peer_exited.connect(
      sigc::mem_fun(*this, &ConfigWindow::on_peer_exited));
  simulator.signal_finished.connect(
      sigc::mem_fun(*this, &ConfigWindow::on_finished));
  simulator.signal_failed.connect(
      sigc::mem_fun(*this, &ConfigWindow::on_failed));

  // Ao fechar a janela principal, encerra o simulador e as janelas de chat
  signal_close_request().connect(
      [this] {
        simulator.stop();
        destroy_chats();
        signals.reset();
        link_view.reset();

        std::error_code ignored;
        std::filesystem::remove(trace_path, ignored);
        return false;
      },
      false);

  set_running(false);
  signals_button.set_sensitive(false);
  link_button.set_sensitive(false);
  update_form();
}

void ConfigWindow::setup_spin(Gtk::SpinButton &spin, double value, double min,
                              double max, double step, int digits) {
  spin.set_range(min, max);
  spin.set_increments(step, step * 10);
  spin.set_digits(digits);
  spin.set_value(value);
  spin.set_numeric(true);
}

void ConfigWindow::add_row(int row, const char *text, Gtk::Widget &widget) {
  auto *label = Gtk::make_managed<Gtk::Label>(text);
  label->set_halign(Gtk::Align::START);

  widget.set_hexpand(true);
  form.attach(*label, 0, row);
  form.attach(widget, 1, row);
}

SimulatorConfig ConfigWindow::read_config() const {
  SimulatorConfig config;

  config.names = {name_a.get_text().raw(), name_b.get_text().raw()};
  config.modem.type =
      PhysicalLayer::Config::all_modem_types().at(modulation.get_selected());
  config.modem.sample_size = (size_t)sample_size_spin.get_value();
  config.modem.bitrate = bitrate_spin.get_value();
  config.modem.amplitude = amplitude_spin.get_value();
  config.modem.carrier_freq = carrier_spin.get_value();
  config.modem.freq_deviation = deviation_spin.get_value();
  config.noise_mu = mu_spin.get_value();
  config.noise_sigma = sigma_spin.get_value();
  const std::array<LinkLayer::FramingType, 4> framing_types = {
      LinkLayer::FramingType::CharacterCount,
      LinkLayer::FramingType::BitInterleaving,
      LinkLayer::FramingType::ByteStuffing,
      LinkLayer::FramingType::BitStuffing};
  const std::array<LinkLayer::CheckType, 5> check_types = {
      LinkLayer::CheckType::None, LinkLayer::CheckType::EvenParity,
      LinkLayer::CheckType::Checksum, LinkLayer::CheckType::CRC32,
      LinkLayer::CheckType::Hamming};
  config.link.framing = framing_types.at(framing.get_selected());
  config.link.check = check_types.at(check_type.get_selected());
  config.link.max_frame_size = (size_t)max_frame_spin.get_value();

  return config;
}

std::string ConfigWindow::check(const SimulatorConfig &config) const {
  if (config.names[0].empty() || config.names[1].empty())
    return "Informe o nome dos dois peers.";
  if (config.names[0] == config.names[1])
    return "Os peers devem ter nomes diferentes.";

  return validate(config.modem);
}

void ConfigWindow::update_form() {
  SimulatorConfig config = read_config();

  // Campos que não se aplicam à modulação escolhida ficam desabilitados
  carrier_spin.set_sensitive(is_passband(config.modem.type));
  deviation_spin.set_sensitive(config.modem.type ==
                               PhysicalLayer::Config::ModemType::FSK);

  std::string error = check(config);
  if (error.empty()) {
    char text[128];
    /**
        Calcula e apresenta o baud do sinal e a taxa de amostragem
        com base nas configurações inseridas.
     */
    std::snprintf(text, sizeof(text),
                  "Baud: %.6g Bd — taxa de amostragem: %.6g Hz",
                  config.modem.baud(),
                  config.modem.baud() * (double)config.modem.sample_size);
    summary_label.set_text(text);
    summary_label.remove_css_class("error");
  } else {
    // Apresenta mensagem de erro
    summary_label.set_text(error);
    summary_label.add_css_class("error");
  }

  /**
     O início do simulador é desabilitado se alguma configuração inválida é
     identificada
  */
  start_button.set_sensitive(error.empty());
}

void ConfigWindow::on_start() {
  // Aplica valores digitados que ainda não foram confirmados
  for (Gtk::SpinButton *spin : spins)
    spin->update();

  SimulatorConfig config = read_config();
  std::string error = check(config);
  if (!error.empty()) {
    set_status(error);
    return;
  }

  destroy_chats();
  signals.reset();
  link_view.reset();

  // O canal recria o arquivo de traço ao iniciar
  std::error_code ignored;
  std::filesystem::remove(trace_path, ignored);
  config.trace_path = trace_path;

  try {
    simulator.start(config);
  } catch (const std::exception &e) {
    set_running(false);
    signals_button.set_sensitive(false);
    link_button.set_sensitive(false);
    set_status(e.what());
    return;
  }

  create_chats(config);

  // A janela de sinais coleta dados desde o início, mas só aparece sob demanda
  signals = std::make_unique<SignalWindow>(
      config.names, config.modem, trace_path,
      std::array<pid_t, 2>{simulator.peer_pid(PEER_A),
                           simulator.peer_pid(PEER_B)});
  signals_button.set_sensitive(true);
  link_view = std::make_unique<LinkWindow>(config.names);
  link_button.set_sensitive(true);

  set_running(true);
  set_status("Simulação em execução.");
}

void ConfigWindow::on_stop() {
  simulator.stop();
  destroy_chats();
  if (signals)
    signals->finish();
  link_view.reset();
  set_running(false);
  set_status("Simulação parada.");
}

void ConfigWindow::on_message(PeerId peer, const std::string &text) {
  if (chats[peer])
    chats[peer]->show_received(text);
}

void ConfigWindow::on_link_event(PeerId peer, const std::string &stage,
                                 const std::string &bits) {
  if (link_view)
    link_view->show_event(peer, stage, bits);
}

void ConfigWindow::on_peer_exited(PeerId peer) {
  if (!chats[peer])
    return;

  chats[peer]->show_system("Conexão encerrada.");
  chats[peer]->set_enabled(false);
}

void ConfigWindow::on_finished() {
  destroy_chats();
  if (signals)
    signals->finish();
  link_view.reset();
  set_running(false);
  set_status("Simulação encerrada: todos os peers saíram.");
}

void ConfigWindow::on_failed(const std::string &reason) {
  destroy_chats();
  if (signals)
    signals->finish();
  link_view.reset();
  set_running(false);
  set_status("Falha na simulação: " + reason);
}

void ConfigWindow::create_chats(const SimulatorConfig &config) {
  for (size_t i = 0; i < chats.size(); i++) {
    chats[i] =
        std::make_unique<ChatWindow>(config.names[i], config.names[1 - i]);
    chats[i]->signal_send.connect([this, i](const std::string &text) {
      if (!simulator.send(PeerId(i), text))
        chats[i]->show_system("Falha ao enviar a mensagem.");
    });
  }

  show_chats();
}

void ConfigWindow::destroy_chats() {
  for (auto &chat : chats)
    chat.reset();
}

void ConfigWindow::show_chats() {
  for (auto &chat : chats)
    if (chat)
      chat->present();
}

void ConfigWindow::show_signals() {
  if (signals)
    signals->present();
}

void ConfigWindow::show_link() {
  if (link_view)
    link_view->present();
}

void ConfigWindow::set_running(bool running) {
  start_button.set_label(running ? "Reiniciar" : "Iniciar");
  stop_button.set_sensitive(running);
  chats_button.set_sensitive(running);
  link_button.set_sensitive(running && link_view != nullptr);
}
