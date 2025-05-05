#ifndef RESPONSE_H
#define RESPONSE_H
#include <libnn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1048576
#define MAX_HEADER 1024
#define MAX_CONTENT_TYPE 128
#define MAX_RESPONSE_CODE 128
#define METHOD_LENGTH 5
#define NUM_OF_MIME_TYPES 10
#define NUM_OF_RESPONSE_CODES 5

char *get_requested_file_path(char *request_buffer);
int construct_header(char **header, int response_code,
                     const char *file_request);
void determine_response_code(const char *request_buffer, char **file_request,
                             int *response_code);

struct mime_type {
  char extension[6];
  char mime_type[50];
};

struct response_code {
  int code;
  char message[50];
};

#endif