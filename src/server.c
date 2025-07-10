#include "server.h"

// The TCP socket set up here is later used for our connections
int init_socket(struct server_config *config) {
  // AF_INET specifies that we are using IPv4 protocols
  // SOCK_STREAM specifies that this is a standard TCP connection
  // SOCK_STREAM only has one valid protocol (IPPROTO_TCP)
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    log_event(ERROR, "Failed to create socket.", log_to_file);
    return -1;
  }

  struct sockaddr_in socket_address;

  socket_address.sin_family = AF_INET;

  // The port must be in network byte order
  socket_address.sin_port = htons(config->port);

  // INADDR_ANY allows the socket to bind to any available interface
  socket_address.sin_addr.s_addr = INADDR_ANY;

  if (bind(sockfd, (struct sockaddr *)&socket_address,
           sizeof(socket_address)) == -1) {
    log_event(ERROR, "Failed to bind socket.", log_to_file);
    return -1;
  }

  if (listen(sockfd, BACKLOG_SIZE) == -1) {
    log_event(ERROR, "Failed to start listening.", log_to_file);
    return -1;
  }
  return sockfd;
}

// Set up the SSL context
// The SSL context is a group of configuration settings for our connections
SSL_CTX *init_ssl_ctx(struct server_config *config) {
  ctx = SSL_CTX_new(TLS_server_method());
  if (!ctx) {
    log_event(ERROR, "Failed to create SSL context.", log_to_file);
    return NULL;
  }

  if (!SSL_CTX_use_certificate_chain_file(ctx, config->cert_path)) {
    log_event(ERROR, "Failed to set certificate.", log_to_file);
    cleanup_ssl();
    return NULL;
  }

  if (!SSL_CTX_use_PrivateKey_file(ctx, config->key_path, SSL_FILETYPE_PEM)) {
    log_event(ERROR, "Failed to set private key.", log_to_file);
    cleanup_ssl();
    return NULL;
  }
  return ctx;
}

// Accept incoming connections, creating a new process for each one
bool client_loop(SSL_CTX *ctx, int sockfd) {
  int clientfd = accept(sockfd, NULL, NULL);
  if (clientfd == -1) {
    log_event(ERROR, "Failed to accept connection.", log_to_file);
    return true;
  }

  // Processes created via forking do not share memory, making them preferable
  // to threads in this instance
  pid_t pid = fork();
  if (pid == -1) {
    log_event(ERROR, "Failed to fork process.", log_to_file);
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
int init_server(struct server_config *config) {
  if (!OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS |
                            OPENSSL_INIT_LOAD_CRYPTO_STRINGS,
                        NULL)) {
    log_event(ERROR, "Failed to initialize OpenSSL globally.", log_to_file);
    return 1;
  }

  SSL_CTX *ctx = init_ssl_ctx(config);

  if (!ctx) {
    return 1;
  }

  sockfd = init_socket(config);

  if (sockfd == -1) {
    log_event(ERROR, "Failed to create socket.", log_to_file);
    return 1;
  }

  char listening_msg[LISTENING_MSG_MAX];
  snprintf(listening_msg, LISTENING_MSG_MAX, "Started listening on port %d.",
           config->port);
  log_event(INFO, listening_msg, log_to_file);

  bool serving_clients = true;
  while (serving_clients) {
    serving_clients = client_loop(ctx, sockfd);
  }

  close(sockfd);

  return 0;
}
