/**
 * paths_security.c
 *
 * Path validation and normalization functions.
 *
 * OVERVIEW:
 * This file provides security-critical path manipulation functions that
 * prevent directory traversal attacks and ensure safe file access.
 *
 * Normalization converts all variants to a standard form, making security
 * checks more reliable.
 */

#include <string.h>

#include "paths_security.h"

/**
 * contains_traversal_patterns - Detect directory traversal attack attempts
 * @file_request: Path from HTTP request to check
 *
 * Checks if the path contains any known directory traversal patterns.
 * This is a critical security function that prevents attackers from
 * accessing files outside the website directory.
 *
 * Return: true if traversal pattern detected, false if path is safe
 */
bool contains_traversal_patterns(const char *file_request) {
  /*
   * We set this array as static const so that it is allocated once and cannot
   * be modified later in the function.
   */
  static const char *traversal_patterns[] = {
      "../",             // Basic traversal
      "%2e%2e%2f",       // Fully URL encoded
      "%2e%2e/",         // Partially encoded (. encoded)
      "..%2f",           // Partially encoded (/ encoded)
      "%2e%2e%5c",       // Encoded with backslash
      "%2e%2e\\",        // Mixed encoding with literal backslash
      "..%5c",           // Windows style, partial encoding
      "%252e%252e%255c", // Double encoded
      "..%255c",         // Mixed double encoding
      "..\\"             // Windows literal backslash
  };
  /*
   * It is standard practice to divide the sizeof (the number of bytes) in an
   * array by the sizeof a single entry to ascertain the number of entries in
   * the array.
   */
  for (size_t i = 0;
       i < sizeof(traversal_patterns) / sizeof(traversal_patterns[0]); ++i) {
    /* Then we search for the substring within the file_request and return true
     if we find it.*/
    if (strstr(file_request, traversal_patterns[i])) {
      return true;
    }
  }

  return false;
}

/**
 * normalize_request_path - Clean up path by removing redundant elements
 * @file_request: Path to normalize (modified in place)
 *
 * Normalizes the path by removing consecutive slashes and trailing slashes,
 * then null terminating the string.
 */
void normalize_request_path(char *file_request) {
  char *src = file_request;
  char *dst = file_request;

  while (*src) {
    if (*src == '/' && (*(src + 1) == '/')) {
      /*
       * By advancing src but not dst, we are ignoring the additional slash
       */
      src++;
    } else {
      /*
       * Copy character from src to dst
       */
      *dst++ = *src++;
    }
  }

  /*
   * Remove trailing slash if present.
   */
  if (dst != file_request && *(dst - 1) == '/') {
    dst--;
  }

  /*
   * We null terminate the string at its new end because string functions expect
   * null terminated strings.
   */
  *dst = '\0';
}
