#ifndef SERVER_H
#define SERVER_H
#include <libnn.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <unistd.h>

#define BACKLOG_SIZE 128
#define BUFFER_SIZE 1048576

extern int sockfd;
extern SSL *ssl;
extern SSL_CTX *ctx;

void SSL_cleanup(void);
int init_server(int *port, char **cert_path, char **key_path);

#endif
