#include "paths.h"

bool website_dir_exists(void) {
  char *path_buffer = malloc(PATH_MAX);
  if (!path_buffer) {
    char malloc_fail_msg[LOG_MAX];
    snprintf(malloc_fail_msg, LOG_MAX,
             "Memory allocation failed for path_buffer: %s", strerror(errno));
    log_event(ERROR, malloc_fail_msg, log_to_file);
    return false;
  }

  prepend_program_data_path(&path_buffer, "website/");
  if (!path_buffer) {
    log_event(ERROR, prepend_err, log_to_file);
    return false;
  }

  if (!file_exists(path_buffer)) {
    log_event(FATAL, "Website directory not found.", log_to_file);
    free(path_buffer);
    return false;
  }

  free(path_buffer);

  return true;
}
