#include "args.h"

void print_usage(void) {
  printf("Usage: cyllenian [options]\n");
  printf("Options:\n");
  printf("  -c               Specify path to certificate file\n");
  printf("  -h               Show this help message\n");
  printf("  -k               Specify path to private key file\n");
  printf("  -l               Save logs to file\n");
  printf("  -p               Specify port to listen on\n");
}

int handle_config_arg(char **config_var, char *optarg) {
  if (*config_var) {
    free(*config_var);
  }

  *config_var = strdup(optarg);
  if (!*config_var) {
    char strdup_fail_msg[LOG_MAX];
    snprintf(strdup_fail_msg, LOG_MAX,
             "Failed to duplicate string to key_path: %s", strerror(errno));
    log_event(ERROR, strdup_fail_msg, log_to_file);
    free(*config_var);
    return 1;
  }

  return 0;
}

void process_args(int argc, char *argv[], struct server_config *config) {
  int c;
  while ((c = getopt(argc, argv, "c:hk:lp:")) != -1) {
    switch (c) {
    case 'c':
      if (handle_config_arg(&config->cert_path, optarg) == 1) {
        exit(EXIT_FAILURE);
      }
      break;

    case 'h':
      print_usage();
      exit(EXIT_SUCCESS);

    case 'k':
      if (handle_config_arg(&config->key_path, optarg) == 1) {
        free(config->cert_path);
        exit(EXIT_FAILURE);
      }
      break;

    case 'l':
      log_to_file = 1;
      break;

    case 'p':
      config->port = atoi(optarg);
      if (config->port <= 1024 || config->port >= 49151) {
        log_event(ERROR, "Port must be between 1024 and 49151.", log_to_file);
        cleanup_config(config);
        exit(EXIT_FAILURE);
      }
      break;

    case '?':
      fprintf(stderr, "Unknown option '-%c'. Run with -h for options.\n",
              optopt);
      char errmsg[LOG_MAX];
      snprintf(errmsg, LOG_MAX,
               "Unknown option '-%c'. Run with -h for options.", optopt);
      log_event(ERROR, errmsg, log_to_file);
      exit(EXIT_FAILURE);
    }
  }
}
