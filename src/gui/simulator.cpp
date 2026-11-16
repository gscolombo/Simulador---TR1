#include "gui/simulator.hpp"
#include "utils.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

using namespace Gui;

namespace {

constexpr size_t LOG_TAIL_SIZE = 400;
constexpr int POLL_INTERVAL_MS = 200;

void close_fd(int &fd) {
  if (fd >= 0)
    ::close(fd);
  fd = -1;
}

std::string describe_status(int status) {
  if (WIFEXITED(status))
    return "terminou com código " + std::to_string(WEXITSTATUS(status));
  if (WIFSIGNALED(status))
    return "foi encerrado pelo sinal " + std::to_string(WTERMSIG(status)) +
           " (" + strsignal(WTERMSIG(status)) + ")";

  return "terminou de forma inesperada";
}

void append_tail(std::string &tail, const char *data, size_t size) {
  tail.append(data, size);
  if (tail.size() > LOG_TAIL_SIZE)
    tail.erase(0, tail.size() - LOG_TAIL_SIZE);
}

} // namespace

Simulator::~Simulator() { stop(); }

/*
  Inicializa simulador com as configurações passadas
*/
void Simulator::start(const SimulatorConfig &config) {
  stop();

  try {
    // Inicia processo do canal
    spawn(CHANNEL, "canal", "channel",
          {number_to_string(config.noise_mu),
           number_to_string(config.noise_sigma), config.trace_path},
          false);

    // Inicia processo dos peers
    for (size_t i = 0; i < 2; i++) {
      std::vector<std::string> args = {config.names[i]};
      const char *framing =
          config.link.framing == LinkLayer::FramingType::CharacterCount
              ? "contagem"
          : config.link.framing == LinkLayer::FramingType::BitInterleaving
              ? "entrelacamento"
          : config.link.framing == LinkLayer::FramingType::ByteStuffing
              ? "byte-stuffing"
              : "bit-stuffing";
      args.push_back(std::string("framing=") + framing);
      args.push_back(std::string("check=") +
                     LinkLayer::check_name(config.link.check));
      args.push_back("max_frame=" + std::to_string(config.link.max_frame_size));
      for (auto &arg : PhysicalLayer::Config::to_args(config.modem))
        args.push_back(arg);

      spawn(i + 1, "peer " + config.names[i], "peer", args, true);
    }
  } catch (...) {
    stop();
    throw;
  }

  // Verificar status de processos-filhos criados a cada POLL_INTERVAL_MS
  // milissegundos
  active = true;
  tick = Glib::signal_timeout().connect(sigc::mem_fun(*this, &Simulator::reap),
                                        POLL_INTERVAL_MS);
}

void Simulator::stop() {
  active = false;
  tick.disconnect();

  for (Child &child : children)
    terminate(child);
}

/*
  Escreve no pipe de entrada de um peer
*/
bool Simulator::send(PeerId peer, const std::string &text) {
  Child &child = children[peer + 1];
  if (!active || child.exited || child.stdin_fd < 0)
    return false;

  std::string line = text + "\n";
  return ::write(child.stdin_fd, line.data(), line.size()) ==
         (ssize_t)line.size();
}

/*
  Inicializa um processo-filho (peer ou channel)
*/
void Simulator::spawn(size_t index, const std::string &label,
                      const std::string &program,
                      const std::vector<std::string> &args, bool is_peer) {
  Child &child = children[index];
  child.label = label;

  int in_pipe[2] = {-1, -1}, out_pipe[2] = {-1, -1}, err_pipe[2] = {-1, -1};

  // Função auxiliar para fechar todos os pipes
  auto close_all = [&] {
    for (int *p : {in_pipe, out_pipe, err_pipe}) {
      close_fd(p[0]);
      close_fd(p[1]);
    }
  };

  // Procura pelo caminho do executável do programa
  using namespace std::filesystem;

  path parent = absolute(read_symlink("/proc/self/exe").parent_path());
  path bin = parent.append(program);

  if (!exists(bin))
    throw std::runtime_error("Programa \"" + program + "\" não encontrado.");

  // Pontas dos pipes são fechadas após exec (O_CLOEXEC), o que
  // garante a entrega de EOF quando um processo termina
  if (pipe2(err_pipe, O_CLOEXEC) < 0 ||
      (is_peer &&
       (pipe2(in_pipe, O_CLOEXEC) < 0 || pipe2(out_pipe, O_CLOEXEC) < 0))) {
    close_all();
    throw std::runtime_error("Erro ao criar pipes para " + label + ".");
  }

  // Redireciona a entrada/saída padrão dos processos filhos para os respectivos
  // pipes, que são fechados após exec (O_CLOEXEC)
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);

  if (is_peer) { // Para peers, stdin é lido do pipe de entrada e stdout é
                 // escrito no pipe de saída
    posix_spawn_file_actions_adddup2(&actions, in_pipe[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, out_pipe[1], STDOUT_FILENO);
  } else {
    // Para o canal, stdin retorna EOF e stdout é escrito no pipe de erro
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null",
                                     O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, err_pipe[1], STDOUT_FILENO);
  }
  // stderr escreve no pipe de erro tanto para os peers quanto para o canal
  posix_spawn_file_actions_adddup2(&actions, err_pipe[1], STDERR_FILENO);

  // Inicializa lista de argumentos
  std::vector<const char *> argv = {program.data()};
  for (const std::string &arg : args)
    argv.push_back(arg.data());
  argv.push_back(nullptr);

  // Cria (spawna) novo processo (peer ou channel)
  int err = posix_spawnp(&child.pid, bin.c_str(), &actions, nullptr,
                         (char *const *)(argv.data()), environ);
  posix_spawn_file_actions_destroy(&actions);

  if (err != 0) {
    close_all();
    throw std::runtime_error("Não foi possível iniciar '" + program +
                             "': " + std::strerror(err));
  }

  // Mantém no processo atual (pai) apenas as pontas dos pipes que ele utiliza
  close_fd(in_pipe[0]);
  close_fd(out_pipe[1]);
  close_fd(err_pipe[1]);

  child.stdin_fd = in_pipe[1];
  child.stdout_fd = out_pipe[0];
  child.stderr_fd = err_pipe[0];

  // Seta a flag O_NONBLOCK para escrita no pipe de entrada do processo-filho
  if (child.stdin_fd >= 0)
    fcntl(child.stdin_fd, F_SETFL, fcntl(child.stdin_fd, F_GETFL) | O_NONBLOCK);

  watch(index);
}

void Simulator::watch(size_t index) {
  Child &child = children[index];
  auto condition = Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP;

  if (child.stdout_fd >= 0)
    child.out_watch = Glib::signal_io().connect(
        [this, index](Glib::IOCondition) { return on_readable(index, false); },
        child.stdout_fd, condition);

  child.err_watch = Glib::signal_io().connect(
      [this, index](Glib::IOCondition) { return on_readable(index, true); },
      child.stderr_fd, condition);
}

void Simulator::terminate(Child &child) {
  child.out_watch.disconnect();
  child.err_watch.disconnect();
  close_fd(child.stdin_fd);
  close_fd(child.stdout_fd);
  close_fd(child.stderr_fd);

  if (child.pid > 0 && !child.exited) {
    kill(child.pid, SIGTERM);

    // Aguarda o término; força o encerramento se não houver resposta
    bool done = false;
    for (int i = 0; i < 100 && !done; i++) {
      done = waitpid(child.pid, nullptr, WNOHANG) != 0;
      if (!done)
        usleep(10000); // Aguarda por 10 milissegundos
    }
    if (!done) { // Força o encerramento
      kill(child.pid, SIGKILL);
      waitpid(child.pid, nullptr, 0);
    }
  }

  child.pid = -1;
  child.exited = false;
  child.out_buffer.clear();
  child.log_tail.clear();
}

bool Simulator::on_readable(size_t index, bool is_stderr) {
  Child &child = children[index];
  int &fd = is_stderr ? child.stderr_fd : child.stdout_fd;

  char buf[4096];
  ssize_t n = ::read(fd, buf, sizeof(buf));

  if (n < 0 && (errno == EINTR || errno == EAGAIN))
    return true;

  if (n <= 0) { // EOF ou erro (-1): o processo fechou a saída
    close_fd(fd);
    return false;
  }

  if (is_stderr || index == CHANNEL)
    append_tail(child.log_tail, buf, n);
  else
    on_stdout_data(index, buf, n);

  return true;
}

void Simulator::on_stdout_data(size_t index, const char *data, size_t size) {
  Child &child = children[index];
  child.out_buffer.append(data, size);

  // Repassa mensagem somente se ela conter uma quebra de linha
  size_t eol;
  while (active && (eol = child.out_buffer.find('\n')) != std::string::npos) {
    std::string line = child.out_buffer.substr(0, eol);
    child.out_buffer.erase(0,
                           eol + 1); // Limpa o buffer do processo filho (peer)

    if (line.rfind("@LINK|", 0) == 0) {
      size_t separator = line.find('|', 6);
      if (separator != std::string::npos)
        signal_link_event.emit(PeerId(index - 1), line.substr(6, separator - 6),
                               line.substr(separator + 1));
    } else {
      signal_message.emit(PeerId(index - 1), line);
    }
  }
}

void Simulator::drain(size_t index) {
  Child &child = children[index];
  char buf[4096];

  for (bool is_stderr : {false, true}) {
    int &fd = is_stderr ? child.stderr_fd : child.stdout_fd;
    ssize_t n;

    while (fd >= 0 && (n = ::read(fd, buf, sizeof(buf))) > 0) {
      if (is_stderr || index == CHANNEL)
        append_tail(child.log_tail, buf, n);
      else
        on_stdout_data(index, buf, n);
    }
  }
}

bool Simulator::reap() {
  if (!active)
    return false;

  bool all_exited = true;

  for (size_t i = 0; i < children.size(); i++) {
    Child &child = children[i];
    if (child.exited)
      continue;

    // Verifica se um processo-filho terminou
    int status = 0;
    if (child.pid <= 0 || waitpid(child.pid, &status, WNOHANG) != child.pid) {
      all_exited = false;
      continue;
    }

    child.exited = true;
    drain(i);
    if (!active) // Algum handler interrompeu o simulador
      return false;

    // Verifica se processo encerrou com erro
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      std::string reason = child.label + " " + describe_status(status);
      if (!child.log_tail.empty())
        reason += ":\n" + child.log_tail;

      fail(reason);
      return false;
    }

    // Desabilita chat se o processo terminado é um dos peers
    if (i != CHANNEL)
      signal_peer_exited.emit(PeerId(i - 1));
  }

  // Para o simulador se todos os processos filhos terminaram
  if (all_exited) {
    stop();
    signal_finished.emit();
    return false;
  }

  return active;
}

void Simulator::fail(const std::string &reason) {
  stop();
  signal_failed.emit(reason);
}
