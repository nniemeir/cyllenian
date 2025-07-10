#include "config.h"

void cleanup_config(struct server_config *config) {
  if (config->cert_path) {
    free(config->cert_path);
  }

  if (config->key_path) {
    free(config->key_path);
  }
}

void init_config(struct server_config *config) {
  config->cert_path = malloc(PATH_MAX);
  if (!config->cert_path) {
    char malloc_fail_msg[LOG_MAX];
    snprintf(malloc_fail_msg, LOG_MAX,
             "Memory allocation failed for cert_path: %s", strerror(errno));
    log_event(ERROR, malloc_fail_msg, log_to_file);
    exit(EXIT_FAILURE);
  }

  config->key_path = malloc(PATH_MAX);
  if (!config->key_path) {
    char malloc_fail_msg[LOG_MAX];
    snprintf(malloc_fail_msg, LOG_MAX,
             "Memory allocation failed for key_path: %s", strerror(errno));
    log_event(ERROR, malloc_fail_msg, log_to_file);
    exit(EXIT_FAILURE);
  }

  if (prepend_program_data_path(&config->cert_path, "cert") == 1 ||
      prepend_program_data_path(&config->key_path, "key") == 1) {
    log_event(ERROR, prepend_err, log_to_file);
    cleanup_config(config);
    exit(EXIT_FAILURE);
  }
}
