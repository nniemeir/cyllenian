/**
 * config.c
 *
 * Server configuration management.
 *
 * OVERVIEW:
 * This file implements a simple configuration system using the Singleton
 * pattern. There's exactly one configuration instance for the entire program,
 * accessed through config_get_ctx().
 */

#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "log.h"
#include "paths.h"

/*
 * 'static' means this variable has file scope - only functions in this
 * file can access it directly. Other files must use config_get_ctx().
 */
static struct server_config config = {0};

/**
 * config_get_ctx - Get pointer to the global runtime configuration
 *
 * This is the accessor function for the singleton config. Any part of
 * the program that needs configuration data calls this function.
 *
 * Return: Pointer to the global configuration structure
 */
struct server_config *config_get_ctx(void) { return &config; }

/**
 * config_cleanup - Free all dynamically allocated configuration memory
 *
 * Frees memory allocated by config_init() and potentially modified by
 * process_args(). This should be called before program exit to prevent
 * memory leaks.
 */
void config_cleanup(void) {
  if (config.cert_path) {
    free(config.cert_path);
  }

  if (config.key_path) {
    free(config.key_path);
  }
}

/**
 * config_init - Initialize configuration with default values
 *
 * Sets up the configuration structure with sensible defaults. These can
 * be overridden later by command-line arguments in process_args().
 *
 * DEFAULT VALUES:
 * - Port: 8080 (common HTTP alternative, doesn't require root)
 * - Certificate: ~/.local/share/cyllenian/cert
 * - Private Key: ~/.local/share/cyllenian/key
 * - Log to file: false (log to stdout by default)
 *
 * PATH CONSTRUCTION:
 * Uses prepend_program_data_path() to construct full paths based on
 * $HOME environment variable. This ensures paths work regardless of
 * which user runs the program.
 *
 * ERROR HANDLING:
 * If memory allocation fails, we log the error and exit immediately.
 * The program can't run without a valid configuration.
 */
int config_init(void) {
  /*
   * Set logging to stdout by default as to not unnecessarily create log files.
   */
  config.log_to_file = false;

  config.port = 8080;

  /*
   * PATH_MAX (4096 bytes) is the maximum path length on Linux.
   * We allocate the full amount because:
   * - We don't know how long the final path will be
   * - PATH_MAX is relatively small (4KB)
   * - Simplifies code (no need to calculate required size)
   *
   * ALLOCATION SIZE:
   * On Linux, PATH_MAX is typically 4096, so this allocates 4096 bytes.
   * Some of it will be wasted (paths are usually < 256 bytes), but
   * 4KB is negligible in a server application.
   */
  config.cert_path = malloc(PATH_MAX);
  if (!config.cert_path) {
    char malloc_fail_msg[LOG_MSG_MAX];
    snprintf(malloc_fail_msg, LOG_MSG_MAX,
             "Memory allocation failed for cert_path: %s", strerror(errno));
    log_event(ERROR, malloc_fail_msg);
    return -1;
  }

  /*
   * Allocate memory for private key path.
   *
   * Same reasoning as cert_path - we need PATH_MAX bytes to safely
   * hold any valid path.
   */
  config.key_path = malloc(PATH_MAX);
  if (!config.key_path) {
    char malloc_fail_msg[LOG_MSG_MAX];
    snprintf(malloc_fail_msg, LOG_MSG_MAX,
             "Memory allocation failed for key_path: %s", strerror(errno));
    log_event(ERROR, malloc_fail_msg);
    return -1;
  }

  /*
   * Construct full paths for certificate and key files.
   *
   * This follows XDG Base Directory specification, which standardizes
   * where applications should store data:
   * - ~/.config: Configuration files
   * - ~/.local/share: Application data
   * - ~/.local/state: Logs and runtime data
   * - ~/.cache: Cached data
   */
  if (prepend_program_data_path(&config.cert_path, "cert") == -1 ||
      prepend_program_data_path(&config.key_path, "key") == -1) {
    config_cleanup();
    return -1;
  }
  return 0;
}
