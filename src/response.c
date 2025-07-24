#include "response.h"
#include "file.h"

int get_response_code_msg(char response_code_msg[MAX_RESPONSE_CODE],
                          int response_code) {
  static const struct response_code
      response_code_associations[NUM_OF_RESPONSE_CODES] = {
          {200, "HTTP/1.1 200 OK"},
          {403, "HTTP/1.1 403 Forbidden"},
          {404, "HTTP/1.1 404 Not Found"},
          {405, "HTTP/1.1 405 Method Not Allowed"}};

  int i;
  for (i = 0; i < NUM_OF_RESPONSE_CODES; ++i) {
    if (response_code == response_code_associations[i].code) {
      snprintf(response_code_msg, MAX_RESPONSE_CODE, "%s\r\n",
               response_code_associations[i].message);
      return 0;
    }
  }

  log_event(ERROR, "Unsupported response code.");

  return 1;
}

// Matches the response code to its corresponding HTTP header
// See Mozilla's HTTP documentation for further details about HTTP response
// codes
char *construct_header(int response_code, const char *file_request) {
  static const char *server_name = "Server: Cyllenian\r\n";
  char *header = malloc(MAX_HEADER);
  if (!header) {
    log_event(ERROR, "Failed to allocate memory for header.");
    return NULL;
  }

  char response_code_msg[MAX_RESPONSE_CODE];
  if (get_response_code_msg(response_code_msg, response_code) == 1) {
    free(header);
    return NULL;
  };

  size_t remaining_header_space = MAX_HEADER;
  size_t response_code_msg_length = strlen(response_code_msg);

  if (response_code_msg_length < remaining_header_space) {
    snprintf(header, MAX_HEADER, "%s", response_code_msg);
    remaining_header_space -= response_code_msg_length;
  } else {
    log_event(ERROR, "Header overflow.");
    free(header);
    return NULL;
  }

  strncat(header, server_name, strlen(server_name));

  char content_type[MAX_CONTENT_TYPE];
  get_content_type(content_type, file_request);

  const size_t content_type_length = strlen(content_type);
  if (content_type_length < remaining_header_space) {
    if (content_type[0] != '\0') {
      strncat(header, content_type,
              remaining_header_space - NULL_TERMINATOR_LENGTH);
    }
    remaining_header_space -= content_type_length;
  } else {
    log_event(ERROR, "Header overflow.");
    free(header);
    return NULL;
  }

  return header;
}

// The requested file path is extracted by skipping over the HTTP method and
// terminating at the next space
char *isolate_file_request(char *request_buffer) {
  if (!request_buffer) {
    log_event(ERROR, "NULL request_buffer was passed to isolate_file_request.");
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

int handle_error_case(char **file_request, char *error_page) {
  static const char *fallback_website_path = "/etc/cyllenian/website";
  free(*file_request);
  *file_request = malloc(PATH_MAX);
  if (!*file_request) {
    char malloc_fail_msg[LOG_MSG_MAX];
    snprintf(malloc_fail_msg, LOG_MSG_MAX,
             "Memory allocation failed for file request: %s", strerror(errno));
    log_event(ERROR, malloc_fail_msg);
    return 0;
  }

  if (!prepend_program_data_path(file_request, "website/")) {
    return 0;
  }

  strcat(*file_request, error_page);

  if (!file_exists(*file_request)) {
    free(*file_request);
    *file_request = malloc(PATH_MAX);
    if (!*file_request) {
      char malloc_fail_msg[LOG_MSG_MAX];
      snprintf(malloc_fail_msg, LOG_MSG_MAX,
               "Memory allocation failed for file request: %s",
               strerror(errno));
      log_event(ERROR, malloc_fail_msg);
      return 0;
    }

    memset(*file_request, 0, PATH_MAX);
    snprintf(*file_request, PATH_MAX, "%s/%s", fallback_website_path,
             error_page);
  }
  return 1;
}

int determine_response_code(const char *request_buffer, char **file_request,
                            int *response_code) {
  const char *method = get_method(request_buffer);
  if (!method) {
    *response_code = 405;
    return handle_error_case(file_request, "405.html");
  }

  normalize_request_path(*file_request);

  if (contains_traversal_patterns(*file_request)) {
    *response_code = 403;
    return handle_error_case(file_request, "403.html");
  }

  if (!file_exists(*file_request)) {
    *response_code = 404;
    return handle_error_case(file_request, "404.html");
  }

  *response_code = 200;

  return 1;
}

int get_requested_file_path(char **path_buffer, char *request_buffer) {
  char *file_request_buffer = strdup(request_buffer);
  if (!file_request_buffer) {
    log_event(ERROR, "Failed to duplicate file_request");
    return 0;
  }

  char *file_request = isolate_file_request(file_request_buffer);
  if (!file_request) {
    file_request = "404.html";
  }

  if (!prepend_program_data_path(path_buffer, "website/")) {
    return 0;
  }

  strcat(*path_buffer, file_request);

  free(file_request_buffer);

  return 1;
}
