/**
 * server.c
 *
 * Core HTTP server implementation handling socket creation, SSL setup,
 * and client connection management.
 *
 * OVERVIEW:
 * This file implements a forking HTTPS server. Each incoming connection
 * spawns a new process to handle the client independently. This approach
 * provides process isolation - if one client causes a crash, other clients
 * continue unaffected.
 */
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "server.h"

/*
 * This is where we store the server variables that may need freed (or closed in
 * the case of the socket file descriptor) on the program's exit.
 */
static struct server_ctx server = {0};

/**
 * server_ctx_init - Initialize server context with sentinel values
 *
 * Sets sockfd to -2 (not -1, because socket() returns that on error)
 * and SSL pointers to NULL. These sentinels allow server_cleanup() to
 * determine which resources have been initialized.
 */
void server_ctx_init(void) {
  server.sockfd = -2;
  server.ssl = NULL;
  server.ssl_ctx = NULL;
}

/**
 * server_cleanup - Free all server resources
 *
 * Cleans up SSL structures and closes socket.
 *
 * The order matters in this case. We free the connection before freeing the
 * context because the former depends on the latter.
 */
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

/**
 * setup_ssl - Establish SSL/TLS connection with client
 * @clientfd: File descriptor for client's TCP connection
 *
 * Creates a new SSL structure for this connection and performs the
 * TLS handshake.
 *
 * After successful handshake, all further communication is encrypted.
 *
 * Return: 0 on success, -1 on failure
 */
int setup_ssl(int clientfd) {
  /*
   * Each connection gets its own SSL structure because each needs unique
   * encryption keys, separate connection state, and independent read/write
   * buffers.
   */
  server.ssl = SSL_new(server.ssl_ctx);
  if (!server.ssl) {
    log_event(ERROR, "Failed to create SSL structure.");
    server_cleanup();
    return -1;
  }

  /*
   * Associate the SSL structure with the client's socket, which OpenSSL will
   * then use when we call SSL_read() and SSL_write(): these functions use the
   * sycalls recv and send for reading and writing to sockets respectively.
   */
  if (!SSL_set_fd(server.ssl, clientfd)) {
    log_event(ERROR, "Failed to set file descriptor for SSL object.");
    server_cleanup();
    return -1;
  }

  /*
   * Perform the TLS handshake. All communication after this point will be
   * encrypted.
   */
  if (SSL_accept(server.ssl) <= 0) {
    log_event(ERROR, "TLS/SSL handshake failed.");
    server_cleanup();
    return -1;
  }
  return 0;
}

/**
 * init_socket - Create and configure TCP listening socket
 *
 * Sets up the server's main socket that listens for incoming connections.
 * This socket doesn't handle client data directly - it only accepts new
 * connections and creates new sockets for them.
 *
 * Return: 0 on success, -1 on failure
 */
static int init_socket(void) {
  /*
   * The AF_INET address family corresponds to IPv4, whereas other applications
   * may use AF_INET6 (IPv6) or AF_UNIX (for Inter-process Communication).
   *
   * The SOCK_STREAM socket type corresponds to TCP, whereas we would use
   * SOCK_DGRAM for UDP.
   *
   * Setting 0 for protocol tells the OS to use the default protocol for the
   * address family and socket type, which is TCP in this case.
   */
  server.sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (server.sockfd == -1) {
    log_event(ERROR, "Failed to create socket.");
    return -1;
  }

  /*
   * Configure the socket's address information.
   */
  struct sockaddr_in socket_address;

  socket_address.sin_family = AF_INET;

  /*
   * Network protocols use big-endian byte order. Some CPUs (like x86)
   * use little-endian, so we convert ot big-endian if needed.
   */
  socket_address.sin_port = htons(config_get_ctx()->port);

  /*
   * Listen on all available network interfaces.
   */
  socket_address.sin_addr.s_addr = INADDR_ANY;

  /*
   * Bind socket to the specified address and port, this routes incoming
   * connections to this socket and prevents other processes from using this
   * port.
   *
   * The cast to (struct sockaddr *) is necessary because bind() uses a
   * generic sockaddr, but we're using the IPv4-specific sockaddr_in.
   */
  if (bind(server.sockfd, (struct sockaddr *)&socket_address,
           sizeof(socket_address)) == -1) {
    log_event(ERROR, "Failed to bind socket.");
    return -1;
  }

  /*
   * Mark socket as passive (listening for connections).
   *
   * BACKLOG_SIZE (128) is the maximum number of pending connections.
   *
   * When clients connect, they enter a queue. If we're busy handling
   * other clients and haven't called accept() yet, new connections wait
   * in this queue. If the queue fills up (128 connections waiting),
   * additional connections are refused.
   *
   * High-traffic servers are likely to use higher values like 1024.
   */
  if (listen(server.sockfd, BACKLOG_SIZE) == -1) {
    log_event(ERROR, "Failed to start listening.");
    return -1;
  }
  return 0;
}

/**
 * init_ssl_ctx - Initialize SSL context with certificate and private key
 *
 * The SSL context (SSL_CTX) is a shared configuration object used by all
 * connections. It contains:
 * - Server certificate (public key)
 * - Server private key
 * - Supported TLS protocol versions
 * - Cipher suite preferences
 * - Session caching settings
 *
 * We use one context for all connections because the certificate and private
 * key don't change between connections. Creating the context once and reusing
 * it is more efficient than reloading these files for every client.
 *
 * Return: 0 on success, -1 on failure
 */
static int init_ssl_ctx(void) {
  /*
   * Create SSL context using the standard TLS server method.
   */
  server.ssl_ctx = SSL_CTX_new(TLS_server_method());
  if (!server.ssl_ctx) {
    log_event(ERROR, "Failed to create SSL context.");
    return -1;
  }

  /*
   * Load the server's certificate chain.
   */
  if (!SSL_CTX_use_certificate_chain_file(server.ssl_ctx,
                                          config_get_ctx()->cert_path)) {
    log_event(ERROR, "Failed to set certificate.");
    server_cleanup();
    return -1;
  }

  /*
   * Load the server's private key. This key is used for decrypting messages
   * encrypted with the public key from the certificate, signing handshake
   * messages to verify the server's identity, and deriving shared encryption
   * keys during the TLS handshake.
   *
   * This file should have permissions 600 (RW for owner, none for others) for
   * the sake of security. SSL_FILETYPE_PEM means the key is in PEM format.
   */
  if (!SSL_CTX_use_PrivateKey_file(server.ssl_ctx, config_get_ctx()->key_path,
                                   SSL_FILETYPE_PEM)) {
    log_event(ERROR, "Failed to set private key.");
    server_cleanup();
    return -1;
  }
  return 0;
}

/**
 * client_loop - Accept and handle a single client connection
 *
 * This function is called repeatedly by server_init(). Each call blocks while
 * waiting for a connection, accepts the connection, and forks a child process
 * to handle it while the parent continues accepting. Most servers use
 * multithreading or event loops these days rather than forking, but this
 * approach is sufficient for a simple low-traffic server.
 *
 * Return: 0 to cease accepting connections, 1 to continue accepting
 * connections, -1 on unrecoverable error
 */
static int client_loop(void) {
  /*
   * We remove a client from the listening queue and create a new socket for
   * them. accept returns the new socket file descriptor on success, or -1 on
   * error.
   */
  int clientfd = accept(server.sockfd, NULL, NULL);
  if (clientfd == -1) {
    log_event(ERROR, "Failed to accept connection.");
    return -1;
  }

  /*
   * Fork a new process to handle this client. Child processes:
   * - Continue from the next line
   * - Copy all variables
   * - Copy open file descriptors
   * - Have a seperate memory space from each other and the parent
   *
   * Both processes can close their own copy of a file
   * descriptor without affecting the other.
   */
  pid_t pid = fork();
  if (pid == -1) {
    log_event(ERROR, "Failed to fork process.");
    close(clientfd);
    return -1;
  }

  /*CHILD PROCESS*/
  if (pid == 0) {
    /*
     * Close the listening socket since we don't need it here and the the port
     * would remain bound if the parent crashed while the child still had the
     * socket open.
     */
    close(server.sockfd);

    handle_client(&server, clientfd);

    return 0; // Exit child process after handling client
  } else {
    /*
     * PARENT PROCESS
     *
     * The parent continues accepting connections. We can close the client
     * socket because the client has its own copy and the parent doesn't need
     * it.
     */
    close(clientfd);
  }
  return 1; // Continue accepting more connections
}

/**
 * server_init - Initialize and run the HTTPS server
 *
 * This initializes OpenSSL, creates the SSL context, opens the listening
 * socket, and enters a client handling loop until a fatal error occurs or
 * SIGINT is received.
 *
 * Return: 0 on success, -1 on error
 */
int server_init(void) {
  /*
   * OpenSSL is humungo so it doesn't initialize everything by
   * default. This call sets up the error message systems and prepares
   * OpenSSL for use.
   *
   * This is a global initialization - call once per program, not per
   * connection or thread.
   */
  if (!OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS |
                            OPENSSL_INIT_LOAD_CRYPTO_STRINGS,
                        NULL)) {
    log_event(ERROR, "Failed to initialize OpenSSL globally.");
    return -1;
  }

  /*
   * Create and configure SSL context.
   * Loads certificate and private key files.
   */
  if (init_ssl_ctx() == -1) {
    return -1;
  }

  /*
   * Create listening socket and bind to port.
   */
  if (init_socket() == -1) {
    log_event(ERROR, "Failed to create socket.");
    return -1;
  }

  char listening_msg[LISTENING_MSG_MAX];
  snprintf(listening_msg, LISTENING_MSG_MAX, "Started listening on port %d.",
           config_get_ctx()->port);
  log_event(INFO, listening_msg);

  /*
   * Accept connections forever (until SIGINT is received) or fatal error
   * occurs.
   */
  int serving_clients = 1;
  while (serving_clients == 1) {
    serving_clients = client_loop();
    if (serving_clients == -1) {
      char unrec_err_msg[LOG_MSG_MAX];
      snprintf(unrec_err_msg, LOG_MSG_MAX,
               "Unrecoverable error in client handling, exiting...\n");
      log_event(FATAL, unrec_err_msg);
    }
  }

  close(server.sockfd);

  return 0;
}
