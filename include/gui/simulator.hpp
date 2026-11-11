#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "link_layer.hpp"
#include "modem.hpp"
#include <array>
#include <glibmm.h>
#include <string>
#include <sys/types.h>

namespace Gui {

enum PeerId { PEER_A = 0, PEER_B = 1 };

// Parâmetros usados para iniciar os processos do simulador
struct SimulatorConfig {
  PhysicalLayer::Config::ModemConfig modem;
  LinkLayer::Config link;
  double noise_mu = 0.;    // V
  double noise_sigma = 1.; // V
  std::array<std::string, 2> names = {"Alice", "Bob"};
  std::string trace_path; // Arquivo de traço do sinal (vazio = desabilitado)
};

/*
  Gerencia os processos do simulador (canal e dois peers). A configuração é
  aplicada na criação dos processos: para alterá-la, o simulador é reiniciado.

  A comunicação com cada peer usa a entrada padrão (mensagem a transmitir, uma
  por linha) e a saída padrão (mensagem recebida, uma por linha).
*/
class Simulator {
public:
  Simulator() = default;
  Simulator(const Simulator &) = delete;
  Simulator &operator=(const Simulator &) = delete;
  ~Simulator();

  // Inicia (ou reinicia) o simulador. Lança std::runtime_error em caso de
  // falha.
  void start(const SimulatorConfig &config);
  void stop();
  bool running() const { return active; }

  // PID do processo de cada peer (identifica o transmissor no traço do sinal)
  pid_t peer_pid(PeerId peer) const { return children[peer + 1].pid; }

  // Envia uma mensagem de texto pelo peer indicado
  bool send(PeerId peer, const std::string &text);

  // Mensagem recebida pelo peer (já decodificada pelas camadas inferiores)
  sigc::signal<void(PeerId, const std::string &)> signal_message;
  sigc::signal<void(PeerId, const std::string &, const std::string &)>
      signal_link_event;
  // Peer encerrou normalmente
  sigc::signal<void(PeerId)> signal_peer_exited;
  // Todos os processos encerraram normalmente
  sigc::signal<void()> signal_finished;
  // Algum processo falhou; o simulador já foi parado
  sigc::signal<void(const std::string &)> signal_failed;

private:
  struct Child {
    std::string label;
    pid_t pid = -1;
    int stdin_fd = -1, stdout_fd = -1, stderr_fd = -1;
    std::string out_buffer; // Linha parcial lida da saída padrão
    std::string log_tail;   // Últimas linhas de log (para mensagens de erro)
    sigc::connection out_watch, err_watch;
    bool exited = false;
  };

  static constexpr size_t CHANNEL = 0; // Índice do canal em `children`

  void spawn(size_t index, const std::string &label, const std::string &program,
             const std::vector<std::string> &args, bool is_peer);
  void terminate(Child &child);
  void watch(size_t index);

  bool on_readable(size_t index, bool is_stderr);
  void on_stdout_data(size_t index, const char *data, size_t size);
  void drain(size_t index);
  bool reap();
  void fail(const std::string &reason);

  std::array<Child, 3> children; // canal, peer A, peer B
  sigc::connection tick;
  bool active = false;
};

} // namespace Gui

#endif
