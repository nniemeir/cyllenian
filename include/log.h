#ifndef LOG_H
#define LOG_H
#include "../../nnlib/nnlib.h"

int log_request(const char *request_buffer, int response_code,
                int response_size);

#endif