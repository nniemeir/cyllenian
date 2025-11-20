/**
 * server.h
 *
 * Core server functionality interface.
 */

#ifndef SERVER_H
#define SERVER_H

#include <openssl/ssl.h>

/**
 * The maximum number of clients that can be waiting
 * in the accept queue while server processes current client. Production servers
 * might allow more than 1024 or more if they are receiving high traffic.
 */
#define BACKLOG_SIZE 128

/**
 * The maximum request/response buffer size. This is also defined in response.h,
 * but we redefine it here for the sake of convenience.
 */
#define BUFFER_SIZE 1048576

/**
 * Maximum length of the initial listening message.
 */
#define LISTENING_MSG_MAX 50

/**
 * struct server_ctx - Server context holding connection state
 *
 * This structure holds the server's connection state. It is declared as
 * file-static in server.c and accessed in other files through
 * initialization/cleanup functions.
 */
struct server_ctx {
  SSL *ssl;
  SSL_CTX *ssl_ctx;
  int sockfd;
};

/**
 * Sets all fields to sentinel values before actual initialization, this is done
 * so that server_cleanup() does not try to free uninitialized memory or close a
 * file descriptor that has not been opened if SIGINT is sent during the startup
 * process.
 */
void server_ctx_init(void);

/**
 * Frees SSL structure and context, then closes the file descriptor that was
 * opened for the socket connection.
 */
void server_cleanup(void);

/**
 * Establish SSL/TLS connection with the client.
 *
 * Return: 0 on success, -1 on failure
 */
int setup_ssl(int clientfd);

/**
 * Complete request-response cycle for one client
 *
 * This runs in the child process. When it returns, the child exits and the OS
 * cleans up remaining resources.
 *
 * Returns early on any error. Child exits after return, so errors here don't
 * affect other clients.
 */
void handle_client(struct server_ctx *server, int clientfd);

/**
 * Initialize and run the HTTPS server. Blocks until a fatal error occurs or
 * SIGINT is received.
 *
 * Return: 0 on clean shutdown, -1 on error
 */
int server_init(void);

#endif
