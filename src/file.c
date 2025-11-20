/**
 * file.c
 *
 * File system utility functions for reading files and extracting metadata.
 *
 * OVERVIEW:
 * This file provides low-level file operations needed by the web server:
 * - Check if file exists
 * - Extract file extension (for determining MIME type)
 * - Read entire file into memory
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "file.h"
#include "log.h"

/**
 * file_exists - Check if a file exists on the filesystem
 * @filename: Full path to file to check
 *
 * Uses stat() system call to check file existence. This is the standard
 * Unix way to check if a file exists.
 *
 *
 * RACE CONDITIONS:
 * There's a TOCTOU (Time Of Check, Time Of Use) race condition here, but we
 * handle fopen() failures gracefully so it is fine.
 *
 * Return: true if file exists, false otherwise
 */
bool file_exists(const char *filename) {
  /*
   * stat() fills this structure with file metadata. We declare it but
   * don't actually use the data - we only care if stat() succeeds.
   */
  struct stat buffer;
  /*
   * Call stat() to check if file exists.
   *
   * We treat all errors as "file doesn't exist" since any possible error means
   * that we can't access it.
   */
  int result = stat(filename, &buffer);
  if (result == -1) {
    return false;
  }

  return true;
}

/**
 * get_file_extension - Extract file extension from path
 * @file_path: Path to file (e.g., "/path/to/file.html")
 *
 * Finds the file extension by searching for the last dot (.) in the path.
 * The extension is used to determine the MIME type (Content-Type header).
 *
 * Return: Pointer to extension (without dot), or NULL if no extension
 */
char *get_file_extension(const char *file_path) {
  /*
   * Find the last occurrence of '.' in the path.
   *
   * strrchr() searches backward from the end of the string.
   * This is different from strchr() which searches forward.
   */
  char *file_extension = strrchr(file_path, '.');
  if (!file_extension) {
    /*
     * No dot found in path.
     */
    return NULL;
  }

  /*
   * Skip past the dot to point at the extension.
   */
  file_extension++;

  /*
   * The pointer we are returning is dependent on file_path and SHOULD NOT be
   * freed, freeing file_path is sufficient.
   */
  return file_extension;
}

/**
 * read_file - Read entire file into memory
 * @file_path: Full path to file to read
 * @file_size: Output parameter - set to number of bytes read
 *
 * Reads the complete contents of a file into a dynamically allocated buffer.
 *
 * Return: Pointer to allocated buffer containing file, or NULL on error
 */
unsigned char *read_file(const char *file_path, size_t *file_size) {
  /*
   * Open file in binary read mode.
   *
   * MODE "rb":
   * - 'r': Read only (we don't need to write)
   * - 'b': Binary mode (no newline translation)
   *
   * WHY BINARY MODE?
   * Text mode ('r' without 'b') on Windows converts \r\n to \n.
   * This corrupts binary files (images, videos, PDFs).
   * Binary mode reads exact bytes from disk.
   *
   * HTML/CSS/JS work fine in binary mode too (they're just text).
   *
   * RETURN VALUES:
   * - Success: Valid FILE* pointer
   * - Failure: NULL (errno set to ENOENT, EACCES, etc.)
   */
  FILE *file = fopen(file_path, "rb");
  if (!file) {
    fprintf(stderr, "Failed to open file %s:%s\n", file_path, strerror(errno));
    return NULL;
  }

  /*
   * Seek to end of file to determine size.
   *
   * WHY SEEK TO END?
   * To find file size, we need to know how far end is from beginning.
   * We seek to end, then use ftell() to get that position (= file size).
   */
  if (fseek(file, 0, SEEK_END) == -1) {
    fprintf(stderr, "Failed to set file position indicator: %s\n",
            strerror(errno));
    fclose(file);
    return NULL;
  }

  /*
   * Get current file position (= file size in bytes).
   *
   * ftell() returns the current position in the file, which is now at
   * the end because we just sought there.
   *
   * We store in 'int' first because ftell() returns long, but we need
   * to check for -1 error before casting to size_t (which is unsigned).
   */
  int size = ftell(file);
  if (size == -1) {
    fprintf(stderr, "Failed to get file position indicator: %s\n",
            strerror(errno));
    if (fclose(file) == -1) {
      fprintf(stderr, "Failed to close file %s:%s\n", file_path,
              strerror(errno));
      return NULL;
    }
  }

  /*
   * Cast to size_t for memory allocation.
   *
   * size_t is the proper type for memory sizes and array indices.
   * ftell() returns long, so we cast.
   *
   * We already know that size != -1, so casting to unsigned is safe here.
   */
  *file_size = (size_t)size;

  /*
   * Seek back to beginning of file.
   *
   * rewind() is equivalent to: fseek(file, 0, SEEK_SET)
   *
   * WHY REWIND?
   * We sought to end to get size, but now we need to read from
   * beginning. rewind() moves position back to start.
   *
   * After this, next fread() will read from byte 0.
   */
  rewind(file);

  unsigned char *buffer = (unsigned char *)malloc(*file_size);
  if (!buffer) {
    fprintf(stderr, "Failed to allocate memory for file buffer: %s\n",
            strerror(errno));

    if (fclose(file) == -1) {
      fprintf(stderr, "Failed to close file %s:%s\n", file_path,
              strerror(errno));
    }
    return NULL;
  }

  const size_t bytes_read = fread(buffer, 1, *file_size, file);

  if (bytes_read != *file_size) {
    log_event(ERROR, "Error reading file into buffer.");
    free(buffer);

    if (fclose(file) == -1) {
      fprintf(stderr, "Failed to close file %s:%s\n", file_path,
              strerror(errno));
    }

    return NULL;
  }

  /*
   * fclose() flushes any buffers and releases the file descriptor.
   */
  if (fclose(file) == -1) {
    fprintf(stderr, "Failed to close file %s:%s\n", file_path, strerror(errno));
    return NULL;
  }

  return buffer;
}
