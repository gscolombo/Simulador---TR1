#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
  int status;
  pid_t channel_p, alice_p, bob_p;

  if ((channel_p = fork()) == -1)
    return -1;
  else if (!channel_p)
    execl("/bin/xterm", "xterm", "-e", "channel", nullptr);

  if ((alice_p = fork()) == -1)
    return -1;
  else if (!alice_p)
    execl("/bin/xterm", "xterm", "-e", "peer", "Alice", nullptr);

  if ((bob_p = fork()) == -1)
    return -1;
  else if (!bob_p)
    execl("/bin/xterm", "xterm", "-e", "peer", "Bob", nullptr);

  while (wait(&status) > 0)
    ;
}