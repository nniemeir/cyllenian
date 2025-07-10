#include "server.h"

// Free the SSL object and context if they are currently allocated
void cleanup_ssl(void) {
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
  if (!ssl) {
    log_event(ERROR, "Failed to create SSL structure.", log_to_file);
    cleanup_ssl();
    return NULL;
  }

  if (!SSL_set_fd(ssl, clientfd)) {
    log_event(ERROR, "Failed to set file descriptor for SSL object.",
              log_to_file);
    cleanup_ssl();
    return NULL;
  }

  if (SSL_accept(ssl) <= 0) {
    log_event(ERROR, "TLS/SSL handshake failed.", log_to_file);
    cleanup_ssl();
    return NULL;
  }
  return ssl;
}
