#include "../include/response.h"
#include "../include/file.h"

char *get_response_code_msg(int response_code) {
  static const struct response_code
      response_code_associations[NUM_OF_RESPONSE_CODES] = {
          {200, "HTTP/1.1 200 OK"},
          {403, "HTTP/1.1 403 Forbidden"},
          {404, "HTTP/1.1 404 Not Found"},
          {405, "HTTP/1.1 405 Method Not Allowed"}};
  int i;
  bool code_match_found = false;
  char *response_code_msg = malloc(MAX_RESPONSE_CODE);
  if (!response_code_msg) {
    log_event(program_name, ERROR,
              "Failed to allocate memory for response_code_msg.", log_to_file);
    return NULL;
  }
  response_code_msg[0] = '\0';

  for (i = 0; i < NUM_OF_RESPONSE_CODES; ++i) {
    if (response_code == response_code_associations[i].code) {
      code_match_found = true;
      snprintf(response_code_msg, MAX_RESPONSE_CODE, "%s\r\n",
               response_code_associations[i].message);
      break;
    }
  }
  if (!code_match_found) {
    log_event(program_name, ERROR, "Unsupported response code.", log_to_file);
    free(response_code_msg);
    return NULL;
  }
  return response_code_msg;
}

char *get_content_type(const char *file_request) {
  static const struct mime_type mime_type_associations[NUM_OF_MIME_TYPES] = {
      {"css", "text/css"},       {"gif", "image/gif"},
      {"htm", "text/html"},      {"html", "text/html"},
      {"jpeg", "image/jpeg"},    {"jpg", "image/jpeg"},
      {"js", "text/javascript"}, {"png", "image/png"},
      {"svg", "image/svg+xml"},  {"ttf", "font/ttf"}};
  char *content_type = malloc(MAX_CONTENT_TYPE);
  if (!content_type) {
    log_event(program_name, ERROR,
              "Failed to allocate memory for content_type.", log_to_file);
    return NULL;
  }
  content_type[0] = '\0';
  char *file_extension = get_file_extension(file_request);
  if (!file_extension) {
    snprintf(content_type, MAX_CONTENT_TYPE,
             "Content-Type: application/octet-stream\r\n\r\n");
    return content_type;
  }
  int i;
  for (i = 0; i < NUM_OF_MIME_TYPES; ++i) {
    if (strcmp(file_extension, mime_type_associations[i].extension) == 0) {
      snprintf(content_type, MAX_CONTENT_TYPE, "Content-Type: %s\r\n\r\n",
               mime_type_associations[i].mime_type);
      break;
    }
  }
  return content_type;
}

// Matches the response code to its corresponding HTTP header
// See Mozilla's HTTP documentation for further details about HTTP response
// codes
int construct_header(char **header, int response_code,
                     const char *file_request) {
  static const char *server_name = "Server: Cyllenian\r\n";
  char *response_code_msg = get_response_code_msg(response_code);
  if (!response_code_msg) {
    return 0;
  }

  size_t used_header_space = 0;
  size_t remaining_header_space = MAX_HEADER;
  size_t response_code_msg_length = strlen(response_code_msg);
  if (response_code_msg_length < remaining_header_space) {
    strncat(*header, response_code_msg, MAX_HEADER);
    used_header_space += response_code_msg_length;
    remaining_header_space -= response_code_msg_length;
  } else {
    log_event(program_name, ERROR, "Header overflow.", log_to_file);
    free(response_code_msg);
    return 0;
  }

  strncat(*header, server_name, strlen(server_name));

  char *content_type = get_content_type(file_request);
  if (!content_type) {
    return 0;
  }

  size_t content_type_length = strlen(content_type);
  if (content_type_length < remaining_header_space) {
    if (content_type[0] != '\0') {
      strncat(*header, content_type,
              remaining_header_space - NULL_TERMINATOR_LENGTH);
    }
    used_header_space += content_type_length;
    remaining_header_space -= content_type_length;
  } else {
    log_event(program_name, ERROR, "Header overflow.", log_to_file);
    free(content_type);
    free(response_code_msg);
    return 0;
  }

  free(content_type);
  free(response_code_msg);
  return 1;
}

// The requested file path is extracted by skipping over the HTTP method and
// terminating at the next space
char *isolate_file_request(char *request_buffer) {
  if (!request_buffer) {
    log_event(program_name, ERROR,
              "NULL request_buffer was passed to isolate_file_request.",
              log_to_file);
    return NULL;
  }

  char *file_request = request_buffer + METHOD_LENGTH;

  char *space_position = strchr(file_request, ' ');
  if (space_position) {
    *space_position = '\0';
  }

  if (file_request[strlen(file_request) - 1] == '/') {
    return NULL;
  }

  return file_request;
}

// Determine if the HTTP method is supported by the server
const char *get_method(const char *request_buffer) {
  if (strncmp(request_buffer, "GET", 3) == 0) {
    return "GET";
  }
  if (strncmp(request_buffer, "HEAD", 4) == 0) {
    return "HEAD";
  }
  return NULL;
}

void determine_response_code(const char *request_buffer, char **file_request,
                             int *response_code) {
  const char *method = get_method(request_buffer);
  if (!method) {
    free(*file_request);
    *file_request = strdup("website/405.html");
    *response_code = 405;
    return;
  }

  normalize_request_path(*file_request);

  if (contains_traversal_patterns(*file_request)) {
    free(*file_request);
    *file_request = strdup("website/403.html");
    *response_code = 403;
    return;
  }

  if (!file_exists(*file_request)) {
    free(*file_request);
    *file_request = strdup("website/404.html");
    *response_code = 404;
    return;
  }

  *response_code = 200;
  return;
}

int get_requested_file_path(char **path_buffer, char *request_buffer) {
  char *file_request_buffer = strdup(request_buffer);
  char *file_request = isolate_file_request(file_request_buffer);
  prepend_program_data_path(program_name, path_buffer, "website/");
  if (!file_request) {
    file_request = "404.html";
  }
  strcat(*path_buffer, file_request);
  free(file_request_buffer);
  return 1;
}
