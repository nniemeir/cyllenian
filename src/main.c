#include "args.h"
#include "config.h"
#include "paths.h"
#include "server.h"
#include "signals.h"

int sockfd;
SSL *ssl;
SSL_CTX *ctx;
int log_to_file;
const char *program_name = "cyllenian";

int main(int argc, char *argv[]) {
  log_to_file = 0;
  struct server_config config;
  config.port = 8080;

  if (init_sig_handler() == 1) {
    exit(EXIT_FAILURE);
  }

  if (!website_dir_exists()) {
    exit(EXIT_FAILURE);
  }

  init_config(&config);

  process_args(argc, argv, &config);

  int server_exit_status = init_server(&config);

  cleanup_config(&config);

  return server_exit_status;
}
