/**
 * file.h
 *
 * Responsible for file operations
 */

#ifndef FILE_H
#define FILE_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Check if file exists using stat() system call
 *
 * RACE CONDITION:
 * There's a TOCTOU race: file could be deleted between us checking if it exists
 * and fopen() being called. We handle fopen() failures gracefully, so this is
 * fine.
 *
 * Return: true if file exists and is accessible, false otherwise
 */
bool file_exists(const char *filename);

/**
 * Returns pointer to file extension without dot for MIME type detection.
 *
 * The return value is a pointer into file_path, DO NOT FREE.
 *
 * Return: Pointer to extension string, or NULL if no extension found
 */
char *get_file_extension(const char *file_path);

/**
 * Reads complete file contents into dynamically allocated buffer. We use
 * unsigned char for binary data (e.g., images) because the range of a byte is
 * 0-255. Using a signed type would restrict us to the range -128–127, which would cause higher
 * values to be misinterpreted as negative numbers and thus corrupt the data.
 * 
 * Return: Pointer to allocated buffer containing file, or NULL on error
 */
unsigned char *read_file(const char *file_path, size_t *file_size);

#endif
