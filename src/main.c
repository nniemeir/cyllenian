#include "args.h"
#include "config.h"
#include "paths.h"
#include "server.h"
#include "signals.h"

int main(int argc, char *argv[]) {
  if (sig_handler_init() == -1) {
    exit(EXIT_FAILURE);
  }

  server_ctx_init();

  if (!website_dir_exists()) {
    exit(EXIT_FAILURE);
  }

  config_init();

  process_args(argc, argv);

  int server_exit_status = server_init();

  config_cleanup();

  return server_exit_status;
}
