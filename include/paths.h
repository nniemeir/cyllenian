#ifndef PATHS_H
#define PATHS_H

#include <stdbool.h>

int prepend_program_data_path(char **path_buffer, const char *original_path);
bool website_dir_exists(void);

#endif
