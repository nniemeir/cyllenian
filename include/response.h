#ifndef RESPONSE_H
#define RESPONSE_H

#include "common.h"

#define BUFFER_SIZE 1048576
#define MAX_HEADER 1024
#define MAX_CONTENT_TYPE 128
#define MAX_RESPONSE_CODE 128
#define METHOD_LENGTH 5
#define NUM_OF_MIME_TYPES 14
#define NUM_OF_RESPONSE_CODES 5

struct mime_type {
  char extension[6];
  char mime_type[50];
};

struct response_code {
  int code;
  char message[50];
};

char *construct_header(int response_code, const char *file_request);
bool determine_response_code(const char *request_buffer, char **file_request,
                             int *response_code);
bool get_requested_file_path(char **path_buffer, char *request_buffer);
void get_content_type(char content_type[MAX_CONTENT_TYPE],
                      const char *file_request);

#endif
