/**
 * args.c
 *
 * Parses CLI arguments that affect the server's runtime configuration.
 *
 * OVERVIEW:
 * It uses getopt(), which is the POSIX specification's standard
 * for parsing command-line arguments, to configure the server's runtime
 * behavior.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "args.h"
#include "config.h"
#include "log.h"

/**
 * print_usage - Display program usage
 *
 * This gets called when -h is specified or when invalid arguments are provided.
 * This information is also included in both the README and the program manual.
 *
 * The output format used here is standard for Unix utilities.
 */
static void print_usage(void) {
  printf("Usage: cyllenian [options]\n");
  printf("Options:\n");
  printf("  -c               Specify path to certificate file\n");
  printf("  -h               Show this help message\n");
  printf("  -k               Specify path to private key file\n");
  printf("  -l               Save logs to file\n");
  printf("  -p               Specify port to listen on\n");
}

/**
 * handle_config_arg - Update a configuration string argument
 * @config_var: Pointer to configuration variable to update
 * @optarg: New value from command-line argument
 *
 * Default values for the configuration values are previously set in
 * config_init, so we free the memory that we allocated to them before
 * duplicating the new string to the variable to avoid memory leaks.
 *
 * strdup returns a malloc'd string, so the variables we assign it's output to
 * need to be freed before the program exits: this is handled by the
 * config_cleanup function that is called when server_init returns or SIGINT
 * (ctrl+c) is received, indicating the program should exit.
 *
 * Return: 0 on success, -1 on error
 */
static int handle_config_arg(char **config_var, char *optarg) {
  if (*config_var) {
    free(*config_var);
  }

  *config_var = strdup(optarg);
  if (!*config_var) {
    char strdup_fail_msg[LOG_MSG_MAX];
    snprintf(strdup_fail_msg, LOG_MSG_MAX,
             "Failed to duplicate string to config variable: %s",
             strerror(errno));
    log_event(ERROR, strdup_fail_msg);
    return -1;
  }

  return 0;
}

/**
 * process_args - Parse command-line arguments and update runtime configuration
 * @argc: Argument count passed from main()
 * @argv: Argument array passed from main()
 *
 * Processes CLI arguments and updates run-time configuration. Uses getopt() for
 * compliance with POSIX argument parsing standard.
 *
 * SUPPORTED OPTIONS:
 *  -c <path>  : Certificate file path
 *  -h         : Help message
 *  -k <path>  : Private key file path
 *  -l         : Enable logging to file
 *  -p <port>  : Port number (1024-49151)
 *
 * PORT RANGES:
 * Ports 0-1023: Reserved for system services (require root)
 * Ports 1024-49151: User ports (our range)
 * Ports 49152-65535: Dynamic/ephemeral ports (used by client connections)
 *
 * Return: 0 if program should exit successfully (-h case), 1 if server
 * initialization should continue, or -1 on error
 */
int process_args(int argc, char *argv[]) {
  /*
   * Get pointer to global configuration structure.
   *
   * The config is created in config_init() and lives for the entire
   * program lifetime. We could technically call config_get_ctx every time we
   * needed to access the struct without a significant performance hit but this
   * approach is more readable.
   */
  struct server_config *config = config_get_ctx();
  int c;

  /*
   * Adding a colon after the letter in getopt's shortopts parameter indicates
   * that the option requires an argument (accessed through optarg).
   * getopt returns -1 when it has finished reading option characters.
   */
  while ((c = getopt(argc, argv, "c:hk:lp:")) != -1) {
    switch (c) {
    case 'c':
      if (handle_config_arg(&config->cert_path, optarg) == -1) {
        config_cleanup();
        return -1;
      }
      break;

    case 'h':
      print_usage();
      return 0;

    case 'k':
      /*
       * Override the default private key path with optarg. The private key must
       * match the certificate specified with -c (or the default).
       *
       * SECURITY NOTE:
       * Private key files should have restrictive permissions (600)
       * to prevent unauthorized access. Anyone with the private key
       * can impersonate your server, WHICH IS REALLY BAD.
       *
       */
      if (handle_config_arg(&config->key_path, optarg) == -1) {
        config_cleanup();
        return -1;
      }
      break;

    case 'l':
      /*
       * Enable logging to daily log files in addition to stdout.
       * These files are stored in ~/.local/state/cyllenian/, as is recommended
       * by the XDG Base Directory Specification.
       */
      config->log_to_file = true;
      break;

    case 'p':
      // Specify which TCP port to listen on.
      config->port = atoi(optarg);
      /*
       * PORT RESTRICTION REASONING:
       * Ports below 1024 are used for crucial system services on Unix, so
       * binding to them requires root access so that users are prevented from
       * impersonating these services.
       *
       * Ports 49152-65535 are assigned automatically by the OS for client
       * connections, so binding to them can create conflicts (though plenty of
       * programs do so).
       */
      if (config->port <= 1024 || config->port >= 49151) {
        log_event(ERROR, "Port must be between 1024 and 49151.");
        config_cleanup();
        return -1;
      }
      break;

    case '?':
      /*
       * getopt returns '?' if it encounters an unknown option (e.g, if we tried
       *  using -q without including it in shortopts). optopt is the actual
       *  character provided.
       */
      fprintf(stderr, "Unknown option '-%c'. Run with -h for options.\n",
              optopt);
      char errmsg[LOG_MSG_MAX];
      snprintf(errmsg, LOG_MSG_MAX,
               "Unknown option '-%c'. Run with -h for options.", optopt);
      log_event(ERROR, errmsg);
      return -1;
    }
  }
  return 1;
}
