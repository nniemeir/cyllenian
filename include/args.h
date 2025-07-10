#ifndef ARGS_H
#define ARGS_H

#include <libnn.h>
#include <unistd.h>
#include "config.h"

void print_usage(void);
void process_args(int argc, char *argv[], struct server_config *config);

#endif
