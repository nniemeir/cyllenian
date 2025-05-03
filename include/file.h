#ifndef FILE_H
#define FILE_H
#include "../../nnlib/nnlib.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const char *program_name;

bool contains_traversal_patterns(const char *file_request);
void normalize_request_path(char *file_request);



#endif