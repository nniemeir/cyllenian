#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>
#include <linux/limits.h>
#include <string.h>
#include <libnn.h>

struct server_config {
  char *cert_path;
  char *key_path;
  int port;
};

void cleanup_config(struct server_config *config);
void init_config(struct server_config *config);

#endif
