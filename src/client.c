#include "log.h"
#include "response.h"
#include "server.h"

int read_from_client(char *request_buffer) {
  int bytes_read = SSL_read(ssl, request_buffer, BUFFER_SIZE - 1);

  if (bytes_read <= 0) {
    log_event(ERROR, "Failed to read from connection.", log_to_file);
    cleanup_ssl();
    return 1;
  }

  request_buffer[bytes_read] = '\0';

  return 0;
}

int process_request(char **path_buffer, char *request_buffer,
                    int *response_code) {
  if (!get_requested_file_path(path_buffer, request_buffer)) {
    log_event(FATAL, "Failed to get requested file path.", log_to_file);
    cleanup_ssl();
    return 1;
  }

  if (!determine_response_code(request_buffer, path_buffer, response_code)) {
    log_event(FATAL, "Failed to determine response code.", log_to_file);
    cleanup_ssl();
    return 1;
  }
  return 0;
}

int write_to_client(char **header, char **path_buffer, size_t *file_size) {
  // After an appropriate response is generated, it is sent to the client
  if (SSL_write(ssl, *header, strlen(*header)) <= 0) {
    log_event(ERROR, "Failed to write header to connection.", log_to_file);
    free(header);
    cleanup_ssl();
    return 1;
  }

  unsigned char *response_file = read_file(*path_buffer, file_size);

  free(*path_buffer);

  if (SSL_write(ssl, response_file, *file_size) <= 0) {
    log_event(ERROR, "Failed to write file to connection.", log_to_file);
    free(response_file);
    cleanup_ssl();
    return 1;
  }

  free(response_file);
  return 0;
}

void handle_client(SSL_CTX *ctx, int clientfd) {
  // Setup SSL connection with client
  ssl = setup_ssl(ctx, clientfd);
  if (!ssl) {
    return;
  }

  char request_buffer[BUFFER_SIZE];
  if (read_from_client(request_buffer) == 1) {
    return;
  }

  char *path_buffer = malloc(PATH_MAX);
  if (!path_buffer) {
    log_event(ERROR, "Failed to allocate memory for path_buffer.", log_to_file);
    cleanup_ssl();
    return;
  }

  int response_code;

  if (process_request(&path_buffer, request_buffer, &response_code) == 1) {
    free(path_buffer);
    return;
  }

  char *header = construct_header(response_code, path_buffer);
  if (!header) {
    log_event(ERROR, "Failed to construct header.", log_to_file);
    cleanup_ssl();
    return;
  }

  size_t file_size;
  write_to_client(&header, &path_buffer, &file_size);

  size_t response_size = strlen(header) + file_size;
  log_request(request_buffer, response_code, response_size);

  free(header);

  // The connection is then closed
  if (SSL_shutdown(ssl) < 0) {
    log_event(ERROR, "Failed to shutdown connection.", log_to_file);
  }

  cleanup_ssl();
  close(clientfd);
}
