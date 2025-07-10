#include "signals.h"

void handler(int signal_num) {
  // printf is not async-signal-safe, so we opt for the write function
  write(STDOUT_FILENO, "\nInterrupt given, closing socket..\n", 36);
  cleanup_ssl();
  close(sockfd);
  exit(EXIT_SUCCESS);
}

int init_sig_handler(void) {
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));

  sa.sa_handler = handler;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    log_event(FATAL, "Failed to configure signal handling", 1);
    return 1;
  }

  return 0;
}
