#ifndef FILE_H
#define FILE_H

#include "common.h"
#include <sys/stat.h>

bool contains_traversal_patterns(const char *file_request);
void normalize_request_path(char *file_request);

bool file_exists(const char *filename);
char *get_file_extension(const char *file_path);
unsigned char *read_file(const char *file_path, size_t *file_size);

#endif
