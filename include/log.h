#ifndef LOG_H
#define LOG_H

#include "common.h"
#include <time.h>

#define LOG_MSG_MAX 1024
#define NULL_TERMINATOR_LENGTH 1

enum levels { DEBUG, INFO, WARN, ERROR, FATAL };

void log_event(int log_level, const char *msg);

// String Operations
void log_request(const char *request_buffer, int response_code,
                 int response_size);

#endif
