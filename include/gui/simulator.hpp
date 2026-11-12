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
  ~Simulator();

  // Inicia (ou reinicia) o simulador. Lança std::runtime_error em caso de
  // falha.
  void start(const SimulatorConfig &config);

  // Para o simulador.
  void stop();

  bool running() const { return active; }

  pid_t peer_pid(PeerId peer) const { return children[peer + 1].pid; }

  /**
    @brief Escreve no pipe de entrada de um peer
    @param peer Identificar do peer (0 ou 1)
    @param text String para envio
  */
  bool send(PeerId peer, const std::string &text);

  // Mensagem recebida pela camada de aplicação de um peer (já decodificada
  // pelas camadas inferiores)
  sigc::signal<void(PeerId, const std::string &)> signal_message;

  // Mensagem recebida pela camada de enlace de um peer
  sigc::signal<void(PeerId, const std::string &, const std::string &)>
      signal_link_event;

  // Peer encerrou normalmente
  sigc::signal<void(PeerId)> signal_peer_exited;

  // Todos os processos encerraram normalmente
  sigc::signal<void()> signal_finished;

  // Algum processo falhou; o simulador já foi parado
  sigc::signal<void(const std::string &)> signal_failed;

private:
  // Estrutura que representa um processo filho
  struct Child {
    std::string label;
    pid_t pid = -1;
    int stdin_fd = -1, stdout_fd = -1, stderr_fd = -1;
    std::string out_buffer; // Linha parcial lida da saída padrão
    std::string log_tail;   // Últimas linhas de log (para mensagens de erro)
    sigc::connection out_watch,
        err_watch; // Handles para monitorar pipes de saída padrão/erro
    bool exited = false;
  };

  static constexpr size_t CHANNEL = 0; // Índice do canal em `children`

  /**
    @brief Cria um processo filho que executa `program`
    @param index Índice do array de estruturas `Child` (`children`)
    @param label Nome atribuído ao processo filho
    @param program Nome do programa que o processo filho irá executar
    @param args Argumentos passados para `program`
    @param is_peer Flag para indicar que o processo filho corresponde a um peer
  */
  void spawn(size_t index, const std::string &label, const std::string &program,
             const std::vector<std::string> &args, bool is_peer);

  /**
    @brief Termina um processo-filho
    @param child Estrutura representando o processo filho a ser terminado
  */
  void terminate(Child &child);

  /**
    @brief Inicializa handlers de callback para monitorar pipes de saída
    padrão/erro
    @param index Índice do array de estruturas `Child` (`children`)
  */
  void watch(size_t index);

  /**
    @brief Callback para leitura de saída de pipe
    @param index Índice do array de estruturas `Child` (`children`)
    @param is_stderr Lê do pipe de erro se verdadeiro
  */
  bool on_readable(size_t index, bool is_stderr);

  /**
    @brief Repassa mensagem recebida pelo peer para o processo pai
    @param index Índice do array de processos filhos (`children`)
    @param data Mensagem recebida em bytes
    @param size Tamanho da mensagem recebida
  */
  void on_stdout_data(size_t index, const char *data, size_t size);

  /**
    @brief Lê o que restou nos pipes de um processo que já terminou
    @param index Índice do array de processos filhos (`children`)
  */
  void drain(size_t index);

  /*
    Verifica se todos os processos (peers e channel)
    estão em execução. Caso contrário, para a simulação
  */
  bool reap();

  /**
    @brief Para simulador e exibe mensagem de erro
    @param reason Mensagem de erro
  */
  void fail(const std::string &reason);

  std::array<Child, 3> children; // canal, peer A, peer B
  sigc::connection
      tick; // Handle de timer para verificar status de processos filhos
  bool active = false;
};

} // namespace Gui

#endif
