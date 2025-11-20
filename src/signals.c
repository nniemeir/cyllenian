/**
 * signals.c
 *
 * Signal handling for graceful server shutdown.
 *
 * OVERVIEW:
 * This file sets up a signal handler for SIGINT (Ctrl+C) to allow clean
 * shutdown of the server.
 */

#include <signal.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "server.h"
#include "signals.h"

static void handler(int signal_num) {
  /*
   * sigaction requires the signal_num parameter for handler functions, but we
   * don't actually need to use it here since we only handle SIGINT. Casting to
   * void prevents the compiler from whining.
   */
  (void)signal_num;

  /*
   * We use the write syscall instead of printf as the latter is not
   * async-signal-safe for a few reasons (uses internal buffering, may allocate
   * memory when large strings are passed to it). strlen is also not
   * async-siginal-safe, so we hardcode the length of the string.
   */
  write(STDOUT_FILENO, "\nInterrupt given, closing socket..\n", 36);

  config_cleanup();

  server_cleanup();

  exit(EXIT_SUCCESS);
}

/**
 * sig_handler_init - Initialize signal handling for SIGINT
 *
 * Registers a custom handler for SIGINT (Ctrl+C). After this, when the
 * user presses Ctrl+C, our handler() function runs instead of the default
 * behavior (immediate termination).
 *
 * Return: 0 on success, -1 on failure
 */
int sig_handler_init(void) {
  struct sigaction sa;

  /*
   * It is generally advisable to initalize a struct by setting all fields to
   * zero as a means of avoiding accessing garbage data.
   */
  memset(&sa, 0, sizeof(sa));

  /*
   * Setting this tells the kernel that we want to run the handler() function
   * when the signal is received.
   */
  sa.sa_handler = handler;

  /*
   * sigaction is what we use to actually register the signal handler.
   */
  if (sigaction(SIGINT, &sa, NULL) == -1) {
    log_event(FATAL, "Failed to configure signal handling");
    return -1;
  }

  return 0;
}
