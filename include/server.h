#ifndef SERVER_H
#define SERVER_H
#include <arpa/inet.h>
#include <libnn.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#define BACKLOG_SIZE 128
#define BUFFER_SIZE 1048576
#define LISTENING_MSG_MAX 50

extern int sockfd;
extern SSL *ssl;
extern SSL_CTX *ctx;

void SSL_cleanup(void);
int init_server(int *port, char **cert_path, char **key_path);

#endif
