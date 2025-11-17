#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "server.h"

static struct server_ctx server = {0};

void server_ctx_init(void) {
  server.sockfd = -2;
  server.ssl = NULL;
  server.ssl_ctx = NULL;
}

void server_cleanup(void) {
  if (server.ssl) {
    SSL_free(server.ssl);
  }

  if (server.ssl_ctx) {
    SSL_CTX_free(server.ssl_ctx);
  }

  if (server.sockfd != -2) {
    close(server.sockfd);
  }
}

// Sets up the SSL structure for client connection
int setup_ssl(int clientfd) {
  server.ssl = SSL_new(server.ssl_ctx);
  if (!server.ssl) {
    log_event(ERROR, "Failed to create SSL structure.");
    server_cleanup();
    return -1;
  }

  if (!SSL_set_fd(server.ssl, clientfd)) {
    log_event(ERROR, "Failed to set file descriptor for SSL object.");
    server_cleanup();
    return -1;
  }

  if (SSL_accept(server.ssl) <= 0) {
    log_event(ERROR, "TLS/SSL handshake failed.");
    server_cleanup();
    return -1;
  }
  return 0;
}

// The TCP socket set up here is later used for our connections
static int init_socket(void) {
  // AF_INET specifies that we are using IPv4 protocols
  // SOCK_STREAM specifies that this is a standard TCP connection
  // SOCK_STREAM only has one valid protocol (IPPROTO_TCP)
  server.sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (server.sockfd == -1) {
    log_event(ERROR, "Failed to create socket.");
    return -1;
  }

  struct sockaddr_in socket_address;

  socket_address.sin_family = AF_INET;

  // The port must be in network byte order
  socket_address.sin_port = htons(config_get_ctx()->port);

  // INADDR_ANY allows the socket to bind to any available interface
  socket_address.sin_addr.s_addr = INADDR_ANY;

  if (bind(server.sockfd, (struct sockaddr *)&socket_address,
           sizeof(socket_address)) == -1) {
    log_event(ERROR, "Failed to bind socket.");
    return -1;
  }

  if (listen(server.sockfd, BACKLOG_SIZE) == -1) {
    log_event(ERROR, "Failed to start listening.");
    return -1;
  }
  return 0;
}

// Set up the SSL context
// The SSL context is a group of configuration settings for our connections
static int init_ssl_ctx(void) {
  server.ssl_ctx = SSL_CTX_new(TLS_server_method());
  if (!server.ssl_ctx) {
    log_event(ERROR, "Failed to create SSL context.");
    return -1;
  }

  if (!SSL_CTX_use_certificate_chain_file(server.ssl_ctx,
                                          config_get_ctx()->cert_path)) {
    log_event(ERROR, "Failed to set certificate.");
    server_cleanup();
    return -1;
  }

  if (!SSL_CTX_use_PrivateKey_file(server.ssl_ctx, config_get_ctx()->key_path,
                                   SSL_FILETYPE_PEM)) {
    log_event(ERROR, "Failed to set private key.");
    server_cleanup();
    return -1;
  }
  return 0;
}

// Accept incoming connections, creating a new process for each one
static int client_loop(void) {
  int clientfd = accept(server.sockfd, NULL, NULL);
  if (clientfd == -1) {
    log_event(ERROR, "Failed to accept connection.");
    return -1;
  }

  // Processes created via forking do not share memory, making them preferable
  // to threads in this instance
  pid_t pid = fork();
  if (pid == -1) {
    log_event(ERROR, "Failed to fork process.");
    close(clientfd);
    return -1;
  }

  if (pid == 0) {
    close(server.sockfd);
    handle_client(&server, clientfd);
    return 0;
  } else {
    close(clientfd);
  }
  return 1;
}

// The main function for HTTPS server
int server_init(void) {
  if (!OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS |
                            OPENSSL_INIT_LOAD_CRYPTO_STRINGS,
                        NULL)) {
    log_event(ERROR, "Failed to initialize OpenSSL globally.");
    return -1;
  }

  if (init_ssl_ctx() == -1) {
    return -1;
  }

  if (init_socket() == -1) {
    log_event(ERROR, "Failed to create socket.");
    return -1;
  }

  char listening_msg[LISTENING_MSG_MAX];
  snprintf(listening_msg, LISTENING_MSG_MAX, "Started listening on port %d.",
           config_get_ctx()->port);
  log_event(INFO, listening_msg);

  int serving_clients = 1;
  while (serving_clients == 1) {
    serving_clients = client_loop();
    if (serving_clients == -1) {
      // TODO: PUT PROPER ERROR LOG HERE
      fprintf(stderr, "CONNECT FAILED, SERVER WILL NOW EXIT");
    }
  }

  close(server.sockfd);

  return 0;
}
