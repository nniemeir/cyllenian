/**
 * paths.c
 *
 * Path construction and validation utilities.
 *
 * OVERVIEW:
 * This file handles building filesystem paths for the server's data
 * directories. It follows the XDG Base Directory specification for portable,
 * user-specific path management.
 */

#include <errno.h>
#include <linux/limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "log.h"

/**
 * prepend_program_data_path - Construct user data directory path
 * @path_buffer: Output buffer to write path into
 * @original_path: Relative path to append (e.g., "website", "cert")
 *
 * We prepend ~/.local/share/cyllenian to the original_path as that directory is
 * where application data lives per the XDG specification.
 *
 * Return: 0 on success, -1 on error
 */
int prepend_program_data_path(char **path_buffer, const char *original_path) {
  /*
   * Get user's home directory from environment. It is unlikely that HOME would
   * not be set on most systems.
   */
  const char *home = getenv("HOME");
  if (!home) {
    log_event(ERROR, "Failed to get value of HOME environment variable.");
    return -1;
  }

  /*
   * You'd have to be a real barnaclehead to encounter this error.
   */
  if (!*path_buffer) {
    log_event(ERROR, "NULL pointer was passed to prepend_program_data_path.");
    return -1;
  }

  snprintf(*path_buffer, PATH_MAX, "%s/.local/share/cyllenian/%s", home,
           original_path);

  return 0;
}

/**
 * website_dir_exists - Verify website directory is present
 *
 * Checks that the website directory exists before starting the server.
 *
 * Return: true if website directory exists, false otherwise
 */
bool website_dir_exists(void) {
  char *path_buffer = malloc(PATH_MAX);
  if (!path_buffer) {
    char malloc_fail_msg[LOG_MSG_MAX];
    snprintf(malloc_fail_msg, LOG_MSG_MAX,
             "Memory allocation failed for path_buffer: %s", strerror(errno));
    log_event(ERROR, malloc_fail_msg);
    return false;
  }

  if (prepend_program_data_path(&path_buffer, "website/") == -1) {
    return false;
  }

  /*
   * We don't distinguish between nonexistent directories and those we don't
   * have permission to access, as we can't access the directory either way.
   */
  if (!file_exists(path_buffer)) {
    log_event(FATAL, "Website directory not found.");
    free(path_buffer);
    return false;
  }

  free(path_buffer);

  return true;
}
