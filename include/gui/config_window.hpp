#ifndef CONFIG_WINDOW_H
#define CONFIG_WINDOW_H

#include "chat_window.hpp"
#include "gtkmm/label.h"
#include "link_window.hpp"
#include "signal_window.hpp"
#include "simulator.hpp"
#include <array>
#include <gtkmm.h>
#include <memory>
#include <vector>

namespace Gui {

/*
  Janela principal: configura o simulador (nomes, modulação, parâmetros do
  modem e ruído) e controla sua execução. Cada peer tem uma janela de chat.
*/
class ConfigWindow : public Gtk::Window {
public:
  /**
    Construtor da janela de configuração. Lê as configurações iniciais padrão
    e inicializa/instancia os componentes de UI para configurar o simulador.
  */
  ConfigWindow();

private:
  // Lê e registra as configurações inseridas nos campos de entrada.
  SimulatorConfig read_config() const;

  /**
    Valida as configurações inseridas e retorna a descrição do
    primeiro problema encontrado (texto vazio se válido).
  */
  std::string check(const SimulatorConfig &config) const;

  /**
    Atualiza o estado da janela para cada alteração das
    configurações.
  */
  void update_form();

  // Callback para iniciar simulador
  void on_start();

  // Callback para parar simulador
  void on_stop();

  /**
    @brief Callback para apresentar mensagem recebida por um dos peers
    @param peer Identificador do peer (0 ou 1)
    @param text Mensagem recebida
  */
  void on_message(PeerId peer, const std::string &text);

  /**
    @brief Callback para apresentar mensagens de log e bitstrings da camada de
    enlace
    @param peer Identificador do peer (0 ou 1)
    @param stage Etapa de processamento (antes/depois do checksum/enquadramento)
    @param bits string de bits ou mensagem de log
  */
  void on_link_event(PeerId peer, const std::string &stage,
                     const std::string &bits);

  // Callback chamada quando um dos peers fecha a conexão
  void on_peer_exited(PeerId peer);

  // Callback chamada quando os dois peers encerram a conexão
  void on_finished();

  // Callback chamada quando ocorre uma falha (fatal) durante a simulação
  void on_failed(const std::string &reason);

  // Instancia duas janelas de chat, uma para cada peer
  void create_chats(const SimulatorConfig &config);

  // Fecha e remove as janelas de chat
  void destroy_chats();

  // Exibe as janelas de chat
  void show_chats();

  // Abre o visualizador de sinais
  void show_signals();

  // Abre o visualizador de bits/logs da camada de enlace
  void show_link();

  // Habilita/desabilita botões caso o simulador esteja em execução/parado
  void set_running(bool running);

  // Seta o texto de estado do simulador
  void set_status(const std::string &text) { status_label.set_text(text); }

  /**
    @brief Adiciona uma linha no formulário com um campo de entrada
    @param row Número da linha
    @param label Rótulo do campo
    @param widget Componente de UI para entrada de usuário
  */
  void add_row(int row, const char *label, Gtk::Widget &widget);

  /**
    @brief Configura as restrições de entrada de um seletor numérico
    @param spin Widget de seleção de valor numérico
    @param value Valor inicial
    @param min Valor mínimo
    @param max Valor máximo
    @param step Valor do passo (incremento/decremento)
    @param digits Número de digitos
  */
  static void setup_spin(Gtk::SpinButton &spin, double value, double min,
                         double max, double step, int digits);

  Simulator simulator;
  std::array<std::unique_ptr<ChatWindow>, 2> chats;
  std::unique_ptr<SignalWindow> signals;
  std::unique_ptr<LinkWindow> link_view;
  std::string trace_path; // Arquivo de registro do sinal escrito pelo canal

  Gtk::Box root{Gtk::Orientation::VERTICAL, 10};
  Gtk::Grid form;
  Gtk::Entry name_a, name_b; // Campos de entrada de texto para nomes dos peers
  Gtk::DropDown modulation, framing, check_type; // Campos de lista dropdown
  Gtk::SpinButton sample_size_spin, bitrate_spin, amplitude_spin, carrier_spin,
      deviation_spin, mu_spin, sigma_spin,
      max_frame_spin; // Seletores de valores numéricos
  std::vector<Gtk::SpinButton *>
      spins; // Vetor de seletores de valores numéricos

  Gtk::Label summary_label; // Texto de baud/taxa de amostragem calculados ou
                            // erro detectado
  Gtk::Label
      status_label; // Texto de estado do simulador (em execução, parado, etc.)
  Gtk::Box buttons{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Button start_button{"Iniciar"}, stop_button{"Parar"},
      chats_button{"Mostrar chats"}, signals_button{"Mostrar sinais"},
      link_button{"Mostrar enlace"};
};

} // namespace Gui

#endif
