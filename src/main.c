#include "../include/server.h"
#include <time.h>

int sockfd;
SSL *ssl;
SSL_CTX *ctx;
int log_to_file;

void handler(int signal_num) {
  // printf is not async-signal-safe, so we opt for the write function
  write(STDOUT_FILENO, "\nInterrupt given, closing socket..\n", 36);
  SSL_cleanup();
  close(sockfd);
  exit(EXIT_SUCCESS);
}

void process_args(int argc, char *argv[], int *port, char **cert_path,
                  char **key_path) {
  int c;
  while ((c = getopt(argc, argv, "c:hk:lp:")) != -1) {
    switch (c) {
    case 'c':
      if (*cert_path) {
        free(*cert_path);
      }
      *cert_path = strdup(optarg);
      if (!*cert_path) {
        log_event(program_name, ERROR,
                  "Failed to allocate memory for cert_path.", log_to_file);
        exit(EXIT_FAILURE);
      }
      break;
    case 'h':
      printf("Usage: cyllenian [options]\n");
      printf("Options:\n");
      printf("  -c               Specify path to certificate file\n");
      printf("  -h               Show this help message\n");
      printf("  -k               Specify path to private key file\n");
      printf("  -l               Save logs to file\n");
      printf("  -p               Specify port to listen on\n");
      exit(EXIT_SUCCESS);
    case 'k':
      if (*key_path) {
        free(*key_path);
      }
      *key_path = strdup(optarg);
      if (!*key_path) {
        log_event(program_name, ERROR,
                  "Failed to allocate memory for key_path.", log_to_file);
        exit(EXIT_FAILURE);
      }
      break;
    case 'l':
      log_to_file = 1;
      break;
    case 'p':
      *port = atoi(optarg);
      if (*port < 1024 || *port > 49151) {
        log_event(
            program_name, ERROR,
            "Invalid port specified, valid ports are between 1024 and 49151.",
            log_to_file);
        exit(EXIT_FAILURE);
      }
      break;
    case '?':
      fprintf(stderr, "Unknown option '-%c'. Run with -h for options.\n",
              optopt);
      char errmsg[LOG_MAX];
      snprintf(errmsg, LOG_MAX,
               "Unknown option '-%c'. Run with -h for options.", optopt);
      log_event(program_name, ERROR, errmsg, log_to_file);
      exit(EXIT_FAILURE);
    }
  }
}

bool website_dir_exists(void) {
  char *path_buffer = malloc(PATH_MAX);
  prepend_program_data_path(program_name, &path_buffer, "website/");
  if (!file_exists(path_buffer)) {
    log_event(program_name, FATAL, "Website directory not found.", log_to_file);
    free(path_buffer);
    return false;
  }
  free(path_buffer);
  return true;
}

int main(int argc, char *argv[]) {
  // These values will be changed if their corresponding arguments are given
  log_to_file = 0;
  int port = 8080;

  if (!website_dir_exists()) {
    return EXIT_FAILURE;
  }

  char *cert_path = malloc(PATH_MAX);
  prepend_program_data_path(program_name, &cert_path, "cert");
  char *key_path = malloc(PATH_MAX);
  prepend_program_data_path(program_name, &key_path, "key");
  if (!cert_path || !key_path) {
    return EXIT_FAILURE;
  }

  process_args(argc, argv, &port, &cert_path, &key_path);

  // sigaction listens for SIGINT (sent by Ctrl+C) for the duration of the
  // process's lifetime
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigaction(SIGINT, &sa, NULL);

  int server_exit_status = init_server(&port, &cert_path, &key_path);
  free(cert_path);
  free(key_path);
  return server_exit_status;
}
