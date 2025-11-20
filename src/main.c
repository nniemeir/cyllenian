/**
 * main.c
 *
 * Entry point for the program.
 *
 * OVERVIEW:
 * This handles the initialization sequence for the web server, setting up
 * all necessary components before entering the main server loop.
 */

#include "args.h"
#include "config.h"
#include "paths.h"
#include "server.h"
#include "signals.h"

/**
 * main - Entry point for the program
 * @argc: Argument count
 * @argv: Argument array
 *
 * Orchestrates the complete lifecycle of the web server.
 *
 * The server runs indefinitely until SIGINT (Ctrl+C) is received or an error
 * occurs. The signal handler ensures graceful shutdown by closing sockets and
 * freeing SSL resources.
 *
 * Return: EXIT_SUCCESS on success, EXIT_FAILURE on failure
 */
int main(int argc, char *argv[]) {
  /*
   * Initialize signal handler for graceful shutdown.
   *
   * The signal handler intercepts SIGINT (Ctrl+C) and ensures we properly
   * close the server socket and free SSL resources before exiting. Without
   * this, the port might remain bound and unavailable for reuse.
   */
  if (sig_handler_init() == -1) {
    exit(EXIT_FAILURE);
  }

  /*
   * Initialize server context with sentinel values so that server_cleanup
   * doesn't try to close an uninitialized socket or free unallocated memory.
   */
  server_ctx_init();

  /*
   * Verify website directory exists so we know that we have content to serve
   * before starting the server.
   *
   * Default location: ~/.local/share/cyllenian/website/
   * Fallback location: /etc/cyllenian/website/
   */
  if (!website_dir_exists()) {
    exit(EXIT_FAILURE);
  }

  /*
   * Initialize configuration with defaults.
   *
   * Sets up default values:
   * - Port: 8080
   * - Certificate path: ~/.local/share/cyllenian/cert
   * - Private key path: ~/.local/share/cyllenian/key
   * - Log to file: false
   *
   * These defaults can be overridden by command-line arguments.
   */
  if (config_init() == -1) {
    config_cleanup();
    exit(EXIT_FAILURE);
  }

  // Parse CLI arguments and modify the runtime configuration accordingly
  int arg_processing_result = process_args(argc, argv);
  if (arg_processing_result == -1) {
    config_cleanup();
    exit(EXIT_FAILURE);
  } else if (arg_processing_result == 0) {
    config_cleanup();
    exit(EXIT_SUCCESS);
  }

  /*
   * Start the server.
   * This is a blocking call that sets up OpenSSL, opens a socket, and listens
   * for connections until SIGINT or fatal error
   *
   * We fork a new process for each client connection for the sake of
   * simplicity, though production servers generally use multithreading.
   */
  int server_exit_status = server_init();

  /*
   * Frees allocated memory for configuration paths, frees resources allocated
   * by OpenSSL, and closes socket file descriptor. This is also called if
   * SIGINT received.
   */
  config_cleanup();

  return server_exit_status;
}
