#include "log.h"

char *get_host(char *host_buffer) {
  char *substring = NULL;

  // Skip the request line
  char *first_line_end = strchr(host_buffer, '\n');
  if (!first_line_end) {
    log_event(ERROR, "Malformed request.", log_to_file);
    return NULL;
  }

  substring = first_line_end + 1;

  // Skip to right after the Host label
  char *host_position = strstr(substring, "Host:");
  if (!host_position) {
    log_event(ERROR, "No host found in request.", log_to_file);
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
char *get_header(char *header_buffer) {
  char *carriage_position = strchr(header_buffer, '\r');
  if (carriage_position) {
    *carriage_position = '\0';
  }
  return header_buffer;
}

// Gather relevant information about the request and send to log_event
int log_request(const char *request_buffer, int response_code,
                int response_size) {
  char *host_buffer = strdup(request_buffer);
  if (!host_buffer) {
    log_event(ERROR, "Failed to duplicate request_buffer.", log_to_file);
    return 0;
  }

  char *host = get_host(host_buffer);
  if (!host) {
    log_event(ERROR, "Failed to extract host from request.", log_to_file);
    free(host_buffer);
    return 0;
  }

  char *header_buffer = strdup(request_buffer);
  if (!header_buffer) {
    log_event(ERROR, "Failed to duplicate request_buffer.", log_to_file);
    free(host_buffer);
    return 0;
  }

  char *header = get_header(header_buffer);
  if (!header) {
    log_event(ERROR, "Failed to extract header from request.", log_to_file);
    free(header);
    free(host_buffer);
  }

  char msg[LOG_MAX];
  snprintf(msg, LOG_MAX, "%s \"%s\" %d %d", host, header, response_code,
           response_size);
  log_event(INFO, msg, log_to_file);

  free(header);
  free(host_buffer);
  return 1;
}
