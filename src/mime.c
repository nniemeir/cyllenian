#include "file.h"
#include "response.h"

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
             "Content-Type: application/octet-stream\r\n\r\n");
    return;
  }

  unsigned int lower_bound = 0;
  unsigned int upper_bound = NUM_OF_MIME_TYPES - 1;
  bool match_found = false;
  int middle_value;

  while (!match_found) {
    if (lower_bound > upper_bound) {
      middle_value = -1;
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
  } else {
    snprintf(content_type, MAX_CONTENT_TYPE,
             "Content-Type: application/octet-stream\r\n\r\n");
  }
}
