#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <openssl/ssl.h>
#include "common.h"
#include "config.h"

#define BACKLOG_SIZE 128
#define BUFFER_SIZE 1048576
#define LISTENING_MSG_MAX 50

struct server_ctx {
  SSL *ssl;
  SSL_CTX *ssl_ctx;
  int sockfd;
};

void server_ctx_init(void);
void server_cleanup(void);
bool setup_ssl(int clientfd);
void handle_client(struct server_ctx *server, int clientfd);
int init_server(void);

#endif
