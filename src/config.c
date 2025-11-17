#include "config.h"
#include "paths.h"

static struct server_config config;

struct server_config *config_get_ctx(void) { return &config; }

void config_cleanup(void) {
  if (config.cert_path) {
    free(config.cert_path);
  }

  if (config.key_path) {
    free(config.key_path);
  }
}

void config_init(void) {
  config.log_to_file = false;

  config.port = 8080;

  config.cert_path = malloc(PATH_MAX);
  if (!config.cert_path) {
    char malloc_fail_msg[LOG_MSG_MAX];
    snprintf(malloc_fail_msg, LOG_MSG_MAX,
             "Memory allocation failed for cert_path: %s", strerror(errno));
    log_event(ERROR, malloc_fail_msg);
    exit(EXIT_FAILURE);
  }

  config.key_path = malloc(PATH_MAX);
  if (!config.key_path) {
    char malloc_fail_msg[LOG_MSG_MAX];
    snprintf(malloc_fail_msg, LOG_MSG_MAX,
             "Memory allocation failed for key_path: %s", strerror(errno));
    log_event(ERROR, malloc_fail_msg);
    exit(EXIT_FAILURE);
  }

  if (prepend_program_data_path(&config.cert_path, "cert") == -1 ||
      prepend_program_data_path(&config.key_path, "key") == -1) {
    config_cleanup();
    exit(EXIT_FAILURE);
  }
}
