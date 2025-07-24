#ifndef LOG_H
#define LOG_H

#include "common.h"
#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define LOG_MSG_MAX 1024
#define NULL_TERMINATOR_LENGTH 1

enum levels { DEBUG, INFO, WARN, ERROR, FATAL };

void log_event(int log_level, const char *msg);

// String Operations
bool prepend_program_data_path(char **path_buffer, const char *original_path);

int log_request(const char *request_buffer, int response_code,
                int response_size);

#endif
