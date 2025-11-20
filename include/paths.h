#ifndef PATHS_H
#define PATHS_H

#include <stdbool.h>

/**
 * Constructs full path in user's data directory following XDG specification:
 * $HOME/.local/share/cyllenian/<original_path>
 *
 * Return: 0 on success, -1 on error
 */
int prepend_program_data_path(char **path_buffer, const char *original_path);

/**
 * Checks that ~/.local/share/cyllenian/website/ exists before starting the
 * server. This is a sanity check - prevents starting server that can't serve
 * files.
 *
 * Return: true if website directory exists, false otherwise
 */
bool website_dir_exists(void);

#endif
