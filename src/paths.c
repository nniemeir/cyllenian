#include "paths.h"
#include "file.h"

int prepend_program_data_path(char **path_buffer, const char *original_path) {
  const char *home = getenv("HOME");
  if (!home) {
    log_event(ERROR, "Failed to get value of HOME environment variable.");
    return -1;
  }

  if (!*path_buffer) {
    log_event(ERROR, "NULL pointer was passed to prepend_program_data_path.");
    return -1;
  }

  snprintf(*path_buffer, PATH_MAX, "%s/.local/share/cyllenian/%s", home,
           original_path);

  return 0;
}

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

  if (!file_exists(path_buffer)) {
    log_event(FATAL, "Website directory not found.");
    free(path_buffer);
    return false;
  }

  free(path_buffer);

  return true;
}
