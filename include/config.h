#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

struct server_config {
  char *cert_path;
  char *key_path;
  int port;
  bool log_to_file;
};

struct server_config *config_get_ctx(void);
void config_cleanup(void);
void config_init(void);

#endif
