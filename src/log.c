#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "config.h"
#include "file.h"
#include "log.h"

static const char *get_log_level_msg(const int log_level) {
  switch (log_level) {
  case DEBUG:
    return "DEBUG";
  case INFO:
    return "INFO";
  case WARN:
    return "WARN";
  case ERROR:
    return "ERROR";
  case FATAL:
    return "FATAL";
  default:
    fprintf(stderr, "Invalid log level supplied.\n");
    return NULL;
  }
}

static int construct_log_path(char **path_buffer) {
  const char *home = getenv("HOME");
  if (!home) {
    log_event(ERROR, "Failed to get value of HOME environment variable.");
    return -1;
  }

  if (!*path_buffer) {
    log_event(ERROR, "NULL pointer was passed to construct_log_path.");
    return -1;
  }

  snprintf(*path_buffer, PATH_MAX, "%s/.local/state/cyllenian", home);

  return 0;
}

static int write_to_log_file(const char *formatted_msg, struct tm *tm) {
  char log_filename[NAME_MAX];

  snprintf(log_filename, NAME_MAX, "log_%d%02d%02d.txt", tm->tm_year + 1900,
           tm->tm_mon + 1, tm->tm_mday);

  char *path_buffer = malloc(PATH_MAX);
  if (!path_buffer) {
    fprintf(stderr, "Failed to allocate memory for path_buffer: %s\n",
            strerror(errno));
    return -1;
  }

  if (construct_log_path(&path_buffer) == -1) {
    free(path_buffer);
    return -1;
  }

  char *log_path = malloc(PATH_MAX);
  if (!log_path) {
    fprintf(stderr, "Failed to allocate memory for log_path: %s\n",
            strerror(errno));
    free(path_buffer);
    return -1;
  }

  if (!file_exists(path_buffer)) {
    if (mkdir(path_buffer, 0700) == -1) {
      fprintf(stderr, "Failed to make log directory: %s\n", strerror(errno));
      free(log_path);
      free(path_buffer);
      return -1;
    }
  }

  snprintf(log_path, PATH_MAX, "%s/%s", path_buffer, log_filename);
  FILE *file = fopen(log_path, "a+");
  if (!file) {
    fprintf(stderr, "Failed to open file %s:%s\n", log_path, strerror(errno));
    free(path_buffer);
    return -1;
  }

  fprintf(file, "%s", formatted_msg);

  if (fclose(file) == -1) {
    fprintf(stderr, "Failed to close file %s:%s\n", log_path, strerror(errno));
    free(path_buffer);
    free(log_path);
    return -1;
  }

  free(path_buffer);
  free(log_path);

  return 0;
}

void log_event(int log_level, const char *msg) {
  if (!msg) {
    fprintf(stderr, "NULL log message.\n");
    return;
  }

  if (msg[0] == '\0') {
    fprintf(stderr, "Empty log message.\n");
    return;
  }

  const char *log_level_msg = get_log_level_msg(log_level);
  if (!log_level_msg) {
    return;
  }

  const time_t t = time(NULL);
  if (t == (time_t)-1) {
    fprintf(stderr, "Failed to get time: %s", strerror(errno));
    return;
  }

  struct tm *tm = localtime(&t);
  if (tm == NULL) {
    fprintf(stderr, "Failed to get time: %s", strerror(errno));
    return;
  }

  char formatted_msg[LOG_MSG_MAX];
  snprintf(formatted_msg, LOG_MSG_MAX, "[%d/%02d/%02d %02d:%02d:%02d] %s  %s\n",
           tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900, tm->tm_hour,
           tm->tm_min, tm->tm_sec, log_level_msg, msg);

  if (log_level > INFO) {
    fprintf(stderr, "%s", formatted_msg);
  } else {
    printf("%s", formatted_msg);
  }

  if (config_get_ctx()->log_to_file) {
    if (write_to_log_file(formatted_msg, tm) == -1) {
      fprintf(stderr, "Failed to write to log file.\n");
      return;
    }
  }
}

static char *get_host(char *host_buffer) {
  char *substring = NULL;

  // Skip the request line
  char *first_line_end = strchr(host_buffer, '\n');
  if (!first_line_end) {
    log_event(ERROR, "Malformed request.");
    return NULL;
  }

  substring = first_line_end + 1;

  // Skip to right after the Host label
  char *host_position = strstr(substring, "Host:");
  if (!host_position) {
    log_event(ERROR, "No host found in request.");
    return NULL;
  }

  host_position += 6;

  // Terminate the string after the hostname
  char *host_end = strchr(host_position, ':');
  if (host_end) {
    *host_end = '\0';
  }

  return host_position;
}

// Extract the header by truncating the response at the first carriage return
static char *get_header(char *header_buffer) {
  char *carriage_position = strchr(header_buffer, '\r');
  if (carriage_position) {
    *carriage_position = '\0';
  }
  return header_buffer;
}

// Gather relevant information about the request and send to log_event
void log_request(const char *request_buffer, int response_code,
                 int response_size) {
  char *host_buffer = strdup(request_buffer);
  if (!host_buffer) {
    log_event(ERROR, "Failed to duplicate request_buffer.");
    return;
  }

  char *host = get_host(host_buffer);
  if (!host) {
    log_event(ERROR, "Failed to extract host from request.");
    free(host_buffer);
    return;
  }

  char *header_buffer = strdup(request_buffer);
  if (!header_buffer) {
    log_event(ERROR, "Failed to duplicate request_buffer.");
    free(host_buffer);
    return;
  }

  char *header = get_header(header_buffer);
  if (!header) {
    log_event(ERROR, "Failed to extract header from request.");
    free(header);
    free(host_buffer);
    return;
  }

  char msg[LOG_MSG_MAX];
  snprintf(msg, LOG_MSG_MAX, "%s \"%s\" %d %d", host, header, response_code,
           response_size);
  log_event(INFO, msg);

  free(header);
  free(host_buffer);
}
