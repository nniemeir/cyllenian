#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <libnn.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include "config.h"

#define BACKLOG_SIZE 128
#define BUFFER_SIZE 1048576
#define LISTENING_MSG_MAX 50

extern int sockfd;
extern SSL *ssl;
extern SSL_CTX *ctx;

void cleanup_ssl(void);
SSL *setup_ssl(SSL_CTX *ctx, int clientfd);
void handle_client(SSL_CTX *ctx, int clientfd);
int init_server(struct server_config *config);

#endif
