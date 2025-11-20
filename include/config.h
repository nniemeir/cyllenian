/**
 * config.h
 *
 * Server configuration management interface.
 *
 * OVERVIEW:
 * Defines the configuration structure and functions for managing
 * server settings.
 *
 * CONFIGURATION STRUCTURE:
 * Contains all runtime settings that can be configured via command-line
 * arguments or defaults.
 *
 * Our server_config instance is a singleton. If functions in other files need
 * to access it, they get a pointer to it with config_get_ctx: this is a cleaner
 * approach than passing it as a function parameter for every function that
 * needs it (gross).
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

/**
 * This structure holds all configurable server settings. Default values for
 * each field are set in config_init.
 */
struct server_config {
  char *cert_path;
  char *key_path;
  int port;
  bool log_to_file;
};

// Get pointer to global configuration
struct server_config *config_get_ctx(void);

// Frees the memory allocated for cert_path and key_path
void config_cleanup(void);

/**
 * config_init - Initialize server_config instance with defaults
 *
 * Allocates memory for paths using malloc(). Memory must be freed
 * with config_cleanup() on shutdown.
 * 
 * Return: 0 on success, -1 on failure
 */
int config_init(void);

#endif
