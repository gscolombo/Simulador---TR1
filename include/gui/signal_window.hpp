#ifndef SIGNAL_WINDOW_H
#define SIGNAL_WINDOW_H

#include "modem.hpp"
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
  /**
    @brief Cria os quatro gráficos (transmitido e recebido, para cada sentido),
    sincroniza zoom e deslocamento de cada par e inicia a atualização
    periódica. Fechar a janela apenas a oculta; a leitura continua.
    @param names Nomes dos peers (índice 0: peer A, índice 1: peer B)
    @param modem Configuração do modem, usada para obter a taxa de
    amostragem (amostras por símbolo x baud) e o tamanho do símbolo
    @param trace_path Arquivo de registro do sinal gravado pelo canal
    @param peer_pids PID de cada peer, na mesma ordem de `names`. Identifica o
    transmissor de cada bloco de sinal no arquivo de registro
  */
  SignalWindow(const std::array<std::string, 2> &names,
               const Config::ModemConfig &modem, const std::string &trace_path,
               const std::array<pid_t, 2> &peer_pids);

  // Interrompe a atualização periódica
  ~SignalWindow() override;

  // Faz a última leitura do registro e interrompe a atualização periódica. Os
  // sinais continuam exibidos
  void finish();

private:
  /**
    @brief Cria um gráfico com legenda, associa a ele as amostras do
    transmissor e etapa indicados e o adiciona à janela
    @param index Posição em `plots` (sentido * 2 + etapa)
    @param caption Legenda exibida acima do gráfico
    @param pid PID do peer transmissor do sinal
    @param stage Etapa do sinal: TRACE_IN (transmitido, antes do ruído) ou
    TRACE_OUT (recebido, depois do ruído)
    @param sample_rate Taxa de amostragem (Hz), usada no eixo do tempo
    @param samples_per_symbol Amostras por símbolo, usadas para marcar os
    limites de símbolo
  */
  void add_plot(size_t index, const std::string &caption, pid_t pid,
                PhysicalLayer::TraceStage stage, double sample_rate,
                size_t samples_per_symbol);

  // Lê os registros novos e redesenha os gráficos se houver dados novos.
  // Sempre retorna true, para o temporizador continuar ativo
  bool on_tick();

  // Declarado antes dos gráficos: eles guardam ponteiros para as amostras do
  // leitor e, por isso, precisam ser destruídos antes dele (os membros são
  // destruídos na ordem inversa da declaração)
  TraceReader reader;

  Gtk::Box root{Gtk::Orientation::VERTICAL, 6};
  Gtk::Label hint; // Instruções de uso dos gráficos
  Gtk::ScrolledWindow scroller;
  Gtk::Box content{Gtk::Orientation::VERTICAL, 4}; // Legendas e gráficos

  // [sentido * 2 + etapa]; sentido: 0 = A -> B e 1 = B -> A;
  // etapa: 0 = transmitido (TRACE_IN) e 1 = recebido (TRACE_OUT)
  std::array<SignalPlot, 4> plots;

  sigc::connection tick; // Temporizador da atualização periódica
};

} // namespace Gui

#endif