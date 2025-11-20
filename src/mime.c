/**
 * mime.c
 *
 * MIME type detection based on file extension.
 *
 * OVERVIEW:
 * MIME (Multipurpose Internet Mail Extensions) types tell browsers how to
 * interpret response data. We could inspect the file's contents to determine
 * it's type, but we keep things simple by just using the file extension and
 * default to text/plain.
 */
#include <stdio.h>
#include <string.h>

#include "file.h"
#include "response.h"

/**
 * get_content_type - Determine Content-Type header for file
 * @content_type: Output buffer for Content-Type header
 * @file_request: Path to file (used to extract extension)
 *
 * Maps file extension to MIME type and constructs complete Content-Type
 * header. Uses binary search for efficiency, so the mime_type_associations
 * array MUST remain in alphabetical order if additional entries are added.
 * We end this line with a DOUBLE CRLF (\r\n\r\n) to indicate that the header
 * ends here.
 */
void get_content_type(char content_type[MAX_CONTENT_TYPE],
                      const char *file_request) {
  static const struct mime_type mime_type_associations[NUM_OF_MIME_TYPES] = {
      {"css", "text/css"},       {"gif", "image/gif"},
      {"htm", "text/html"},      {"html", "text/html"},
      {"jpeg", "image/jpeg"},    {"jpg", "image/jpeg"},
      {"js", "text/javascript"}, {"json", "application/json"},
      {"mp3", "audio/mpeg"},     {"mp4", "video/mp4"},
      {"png", "image/png"},      {"svg", "image/svg+xml"},
      {"ttf", "font/ttf"},       {"xml", "application/xml"}};

  char *file_extension = get_file_extension(file_request);
  if (!file_extension) {
    snprintf(content_type, MAX_CONTENT_TYPE,
             "Content-Type: text/plain\r\n\r\n");
    return;
  }

  unsigned int lower_bound = 0;
  unsigned int upper_bound = NUM_OF_MIME_TYPES - 1;
  bool match_found = false;
  int middle_value;

  while (!match_found) {
    if (lower_bound > upper_bound) {
      middle_value = -1; // Sentinel for "not found"
      break;
    }

    middle_value = (lower_bound + upper_bound) / 2;

    int comparison_result =
        strcmp(mime_type_associations[middle_value].extension, file_extension);

    if (comparison_result < 0) {
      lower_bound = middle_value + 1;
      continue;
    }

    if (comparison_result == 0) {
      match_found = true;
      break;
    }

    if (comparison_result > 0) {
      upper_bound = middle_value - 1;
      continue;
    }
  }

  if (match_found) {
    snprintf(content_type, MAX_CONTENT_TYPE, "Content-Type: %s\r\n\r\n",
             mime_type_associations[middle_value].mime_type);
  }
}
