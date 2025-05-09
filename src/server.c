#include "../include/server.h"
#include "../include/file.h"
#include "../include/log.h"
#include "../include/response.h"

// Free the SSL object and context if they are currently allocated
void SSL_cleanup(void) {
  if (ssl) {
    SSL_free(ssl);
  }
  if (ctx) {
    SSL_CTX_free(ctx);
  }
}

// Sets up the SSL structure for client connection
SSL *setup_ssl(SSL_CTX *ctx, int clientfd) {
  ssl = SSL_new(ctx);
  if (ssl == NULL) {
    log_event(program_name, ERROR, "Failed to create SSL structure.",
              log_to_file);
    SSL_cleanup();
    return NULL;
  }

  if (!SSL_set_fd(ssl, clientfd)) {
    log_event(program_name, ERROR,
              "Failed to set file descriptor for SSL object.", log_to_file);
    SSL_cleanup();
    return NULL;
  }

  if (SSL_accept(ssl) <= 0) {
    log_event(program_name, ERROR, "TLS/SSL handshake failed.", log_to_file);
    SSL_cleanup();
    return NULL;
  }
  return ssl;
}

void handle_client(SSL_CTX *ctx, int clientfd) {
  // Setup SSL connection with client
  ssl = setup_ssl(ctx, clientfd);
  if (!ssl) {
    return;
  }

  // Read the data sent by the client
  char request_buffer[BUFFER_SIZE];
  int bytes_read = SSL_read(ssl, request_buffer, sizeof(request_buffer) - 1);
  if (bytes_read <= 0) {
    log_event(program_name, ERROR, "Failed to read from connection.",
              log_to_file);
    SSL_cleanup();
    return;
  }
  request_buffer[bytes_read] = '\0';

  char *path_buffer = malloc(4096);
  if (!path_buffer) {
    log_event(program_name, ERROR, "Failed to allocate memory for path_buffer.",
              log_to_file);
    SSL_cleanup();
    return;
  }

  get_requested_file_path(&path_buffer, request_buffer);

  int response_code;

  determine_response_code(request_buffer, &path_buffer, &response_code);

  char *header = malloc(MAX_HEADER);
  if (!header) {
    log_event(program_name, ERROR, "Failed to allocate memory for header.",
              log_to_file);
    SSL_cleanup();
    return;
  }
  header[0] = '\0';

  if (!construct_header(&header, response_code, path_buffer)) {
    log_event(program_name, ERROR, "Failed to construct header.", log_to_file);
    free(header);
    return;
  }

  // After an appropriate response is generated, it is sent to the client
  if (SSL_write(ssl, header, strlen(header)) <= 0) {
    log_event(program_name, ERROR, "Failed to write header to connection.",
              log_to_file);
    free(header);
    SSL_cleanup();
    return;
  }

  size_t file_size;
  unsigned char *response_file =
      read_file(program_name, path_buffer, &file_size);

  free(path_buffer);

  // After an appropriate response is generated, it is sent to the client
  if (SSL_write(ssl, response_file, file_size) <= 0) {
    log_event(program_name, ERROR, "Failed to write file to connection.",
              log_to_file);
    free(response_file);
    SSL_cleanup();
    return;
  }

  free(response_file);

  size_t response_size = strlen(header) + file_size;
  log_request(request_buffer, response_code, response_size);

  free(header);

  // The connection is then closed
  if (SSL_shutdown(ssl) < 0) {
    log_event(program_name, ERROR, "Failed to shutdown connection.",
              log_to_file);
  }

  SSL_cleanup();
  close(clientfd);
}

// The TCP socket set up here is later used for our connections
int init_socket(int port) {
  // AF_INET specifies that we are using IPv4 protocols
  // SOCK_STREAM specifies that this is a standard TCP connection
  // SOCK_STREAM only has one valid protocol (IPPROTO_TCP)
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    log_event(program_name, ERROR, "Failed to create socket.", log_to_file);
    return -1;
  }

  struct sockaddr_in socket_address;
  socket_address.sin_family = AF_INET;
  // The port must be in network byte order
  socket_address.sin_port = htons(port);
  // INADDR_ANY allows the socket to bind to any available interface
  socket_address.sin_addr.s_addr = INADDR_ANY;

  if (bind(sockfd, (struct sockaddr *)&socket_address,
           sizeof(socket_address)) == -1) {
    log_event(program_name, ERROR, "Failed to bind socket.", log_to_file);
    return -1;
  }

  if (listen(sockfd, BACKLOG_SIZE) == -1) {
    log_event(program_name, ERROR, "Failed to start listening.", log_to_file);
    return -1;
  }
  return sockfd;
}

// Set up the SSL context
// The SSL context is a group of configuration settings for our connections
SSL_CTX *init_ssl_ctx(char **cert_path, char **key_path) {
  ctx = SSL_CTX_new(TLS_server_method());
  if (ctx == NULL) {
    log_event(program_name, ERROR, "Failed to create SSL context.",
              log_to_file);
    return NULL;
  }

  if (!SSL_CTX_use_certificate_chain_file(ctx, *cert_path)) {
    log_event(program_name, ERROR, "Failed to set certificate.", log_to_file);
    SSL_cleanup();
    return NULL;
  }

  if (!SSL_CTX_use_PrivateKey_file(ctx, *key_path, SSL_FILETYPE_PEM)) {
    log_event(program_name, ERROR, "Failed to set private key.", log_to_file);
    SSL_cleanup();
    return NULL;
  }
  return ctx;
}

// Accept incoming connections, creating a new process for each one
bool client_loop(SSL_CTX *ctx, int sockfd) {
  int clientfd = accept(sockfd, NULL, NULL);
  if (clientfd == -1) {
    log_event(program_name, ERROR, "Failed to accept connection.", log_to_file);
    return true;
  }

  // Processes created via forking do not share memory, making them preferable
  // to threads in this instance
  pid_t pid = fork();
  if (pid == -1) {
    log_event(program_name, ERROR, "Failed to fork process.", log_to_file);
    close(clientfd);
    return true;
  }

  if (pid == 0) {
    close(sockfd);
    handle_client(ctx, clientfd);
    return false;
  } else {
    close(clientfd);
  }
  return true;
}

// The main function for HTTPS server
int init_server(int *port, char **cert_path, char **key_path) {
  if (!OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS |
                            OPENSSL_INIT_LOAD_CRYPTO_STRINGS,
                        NULL)) {
    log_event(program_name, ERROR, "Failed to initialize OpenSSL globally.",
              log_to_file);
    return EXIT_FAILURE;
  }

  SSL_CTX *ctx = init_ssl_ctx(cert_path, key_path);

  if (!ctx) {
    return EXIT_FAILURE;
  }

  sockfd = init_socket(*port);

  if (sockfd == -1) {
    log_event(program_name, ERROR, "Failed to create socket.", log_to_file);
    return EXIT_FAILURE;
  }

  char listening_msg[LISTENING_MSG_MAX];
  snprintf(listening_msg, LISTENING_MSG_MAX, "Started listening on port %d.",
           *port);
  log_event(program_name, INFO, listening_msg, log_to_file);

  bool serving_clients = true;
  while (serving_clients) {
    serving_clients = client_loop(ctx, sockfd);
  }

  close(sockfd);
  return EXIT_SUCCESS;
}