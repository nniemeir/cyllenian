/**
 * log.c
 *
 * Logging system for server events and HTTP requests.
 *
 * OVERVIEW:
 * This file implements a dual-output logging system that can write to both
 * stdout/stderr and daily log files. Logs include timestamps, severity levels,
 * and formatted messages. Log filenames include the current date, which keeps
 * things organized while preventing log files from growing too large.
 *
 * LOG FORMAT:
 * [MM/DD/YYYY HH:MM:SS] LEVEL  Message
 */

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

/**
 * get_log_level_msg - Convert log level enum to string
 * @log_level: Log level (DEBUG, INFO, WARN, ERROR, FATAL)
 *
 * Maps log levels to human-readable strings for display.
 *
 * LOG LEVEL MEANINGS:
 *
 * DEBUG: Verbose information for debugging
 *
 * INFO: Normal operational messages
 *
 * WARN: Warnings that don't stop operation
 *
 * ERROR: Recoverable errors
 *
 * FATAL: Unrecoverable errors requiring shutdown
 *
 * Return: String representation of log level, or NULL if invalid
 */
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
    /*
     * Invalid log level passed.
     *
     * This shouldn't happen if log_event() is called correctly.
     * Indicates programming error.
     */
    fprintf(stderr, "Invalid log level supplied.\n");
    return NULL;
  }
}

/**
 * construct_log_path - Build path to log file directory
 * @path_buffer: Output buffer for path
 *
 * Constructs path to directory where log files are stored:
 * $HOME/.local/state/cyllenian
 *
 * This follows XDG Base Directory specification:
 * - ~/.local/state: Application state data (logs, history, etc.)
 * - Different from ~/.local/share (permanent data)
 * - Different from ~/.cache (disposable data)
 *
 * Return: 0 on success, -1 if $HOME not set or buffer is NULL
 */
static int construct_log_path(char **path_buffer) {
  /*
   * Same as prepend_program_data_path(), we need $HOME to construct
   * user-specific paths.
   */
  const char *home = getenv("HOME");
  if (!home) {
    log_event(ERROR, "Failed to get value of HOME environment variable.");
    return -1;
  }

  /*
   * Prevent NULL dereference if caller passes bad pointer.
   */
  if (!*path_buffer) {
    log_event(ERROR, "NULL pointer was passed to construct_log_path.");
    return -1;
  }

  snprintf(*path_buffer, PATH_MAX, "%s/.local/state/cyllenian", home);

  return 0;
}

/**
 * write_to_log_file - Append log message to daily log file
 * @formatted_msg: Complete formatted log message (with timestamp, level)
 * @tm: Time structure (used to determine log filename)
 *
 * Writes log message to appropriate daily log file. Creates directory and
 * file if they don't exist.
 *
 * Return: 0 on success, -1 on error
 */
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
    /*
     * Create log directory with permissions 0700 (Owner has RWX, other users
     * have no access) as these logs contain sensitive data. mkdir doesn't
     * automatically create parent directories, but we can safely assume that
     * ~/.local/state/ exists on Linux systems.
     */
    if (mkdir(path_buffer, 0700) == -1) {
      fprintf(stderr, "Failed to make log directory: %s\n", strerror(errno));
      free(log_path);
      free(path_buffer);
      return -1;
    }
  }

  snprintf(log_path, PATH_MAX, "%s/%s", path_buffer, log_filename);
  /*
   * Open log file in append mode so that we don't overwrite previous logs if
   * the file already exists.
   */
  FILE *file = fopen(log_path, "a");
  if (!file) {
    fprintf(stderr, "Failed to open file %s:%s\n", log_path, strerror(errno));
    free(path_buffer);
    return -1;
  }

  /*
   * Write log message to file.
   *
   * fprintf() is like printf() but outputs to file instead of stdout. fprintf()
   * is buffered, so messages might not actually be written until fclose() is
   * called.
   */
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

/**
 * log_event - Log a message with timestamp and level
 * @log_level: Severity level (DEBUG, INFO, WARN, ERROR, FATAL)
 * @msg: Message to log
 *
 * Main logging function. Formats message with timestamp and level, then
 * outputs to console and optionally to file. INFO and DEBUG are printed to
 * stdout, all other log levels print to stderr.
 *
 * TIMESTAMP FORMAT:
 * [MM/DD/YYYY HH:MM:SS] LEVEL  Message
 */
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

  /*
   * time() gives us the number of seconds since Unix epoch (Jan 1, 1970
   * 00:00:00 UTC).
   */
  const time_t t = time(NULL);
  if (t == (time_t)-1) {
    fprintf(stderr, "Failed to get time: %s", strerror(errno));
    return;
  }

  /*
   * localtime() converts time_t (seconds since epoch) to broken-down time
   * in local timezone.
   */
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

/**
 * get_host - Extract hostname from HTTP request
 * @host_buffer: Copy of request buffer to parse
 *
 * Extracts the Host header value from HTTP request for logging. This header is
 * mandatory in the HTTP/1.1 specification and logging it helps us in tracking
 * connections.
 *
 * Return: Pointer to hostname, or NULL if not found
 */
static char *get_host(char *host_buffer) {
  char *substring = NULL;

  /*
   * Find first newline to skip to headers section.
   */
  char *first_line_end = strchr(host_buffer, '\n');
  if (!first_line_end) {
    log_event(ERROR, "Malformed request.");
    return NULL;
  }

  substring = first_line_end + 1; // Start of headers

  /*
   * strstr() returns pointer to 'H' in "Host:".
   */
  char *host_position = strstr(substring, "Host:");
  if (!host_position) {
    log_event(ERROR, "No host found in request.");
    return NULL;
  }

  /*
   * Skip past "Host: " to get hostname.
   */
  host_position += 6;

  /*
   * Sometimes the host is shown with a port appended, but we opt to ignore it
   * since we already know it.
   */
  char *host_end = strchr(host_position, ':');
  if (host_end) {
    *host_end = '\0';
  }

  return host_position;
}

/**
 * get_header - Extract request line from HTTP request
 * @header_buffer: Copy of request buffer to parse
 *
 * Extracts the first line of HTTP request for logging.
 *
 * Return: Pointer to request line, or original buffer if \r not found
 */
static char *get_header(char *header_buffer) {
  char *carriage_position = strchr(header_buffer, '\r');
  if (carriage_position) {
    *carriage_position = '\0';
  }
  return header_buffer;
}

/**
 * log_request - Log HTTP request in standard format
 * @request_buffer: Complete HTTP request from client
 * @response_code: HTTP status code we returned (200, 404, etc.)
 * @response_size: Total bytes sent (headers + body)
 *
 * Logs HTTP request in format similar to Common Log Format, which many web
 * servers use.
 *
 * [timestamp] INFO hostname "method path" status_code bytes_sent
 */
void log_request(const char *request_buffer, int response_code,
                 int response_size) {
  /*
   * Duplicate request buffer for parsing hostname, since we'll need the
   * original for get_header.
   */
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
