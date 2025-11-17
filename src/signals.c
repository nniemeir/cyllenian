#include <signal.h>
#include <unistd.h>

#include "log.h"
#include "server.h"
#include "signals.h"

static void handler(int signal_num) {
  // printf is not async-signal-safe, so we opt for the write function
  (void)signal_num;
  write(STDOUT_FILENO, "\nInterrupt given, closing socket..\n", 36);
  server_cleanup();
  exit(EXIT_SUCCESS);
}

int sig_handler_init(void) {
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));

  sa.sa_handler = handler;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    log_event(FATAL, "Failed to configure signal handling");
    return -1;
  }

  return 0;
}
