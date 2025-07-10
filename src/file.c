#include "file.h"

bool contains_traversal_patterns(const char *file_request) {
  // Checks for common directory traversal patterns and their encoded forms,
  // which could otherwise be used to access files outside of the website
  // directory
  static const char *traversal_patterns[] = {
      "../",      "%2e%2e%2f", "%2e%2e/",         "..%2f",   "%2e%2e%5c",
      "%2e%2e\\", "..% 5c ",   "%252e%252e%255c", "..%255c", "..\\"};

  for (size_t i = 0;
       i < sizeof(traversal_patterns) / sizeof(traversal_patterns[0]); ++i) {
    if (strstr(file_request, traversal_patterns[i])) {
      return true;
    }
  }

  return false;
}

// Consecutive slashes are skipped, trailing slashes are removed, and the string
// is null terminated
void normalize_request_path(char *file_request) {
  char *src = file_request;
  char *dst = file_request;

  while (*src) {
    if (*src == '/' && (*(src + 1) == '/')) {
      src++;
    } else {
      *dst++ = *src++;
    }
  }

  if (dst != file_request && *(dst - 1) == '/') {
    dst--;
  }

  *dst = '\0';
}
