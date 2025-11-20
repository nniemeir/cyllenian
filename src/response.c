/**
 * response.c
 *
 * Responsible for HTTP response generation and request validation.
 */

#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "log.h"
#include "paths.h"
#include "paths_security.h"
#include "response.h"

/**
 * get_response_code_msg - Convert status code to HTTP status line
 * @response_code_msg: Buffer to store status line
 * @response_code: HTTP status code (200, 403, 404, 405)
 *
 * Return: 0 on success, -1 on unsupported status code
 */
static int get_response_code_msg(char response_code_msg[MAX_RESPONSE_CODE],
                                 int response_code) {
  static const struct response_code
      response_code_associations[NUM_OF_RESPONSE_CODES] = {
          {200, "HTTP/1.1 200 OK"},
          {403, "HTTP/1.1 403 Forbidden"},
          {404, "HTTP/1.1 404 Not Found"},
          {405, "HTTP/1.1 405 Method Not Allowed"}};

  /*
   * Since the array is very small, linear search is fine.
   */
  int i;
  for (i = 0; i < NUM_OF_RESPONSE_CODES; ++i) {
    if (response_code == response_code_associations[i].code) {
      snprintf(response_code_msg, MAX_RESPONSE_CODE, "%s\r\n",
               response_code_associations[i].message);
      return 0;
    }
  }

  log_event(ERROR, "Unsupported response code.");

  return -1;
}

/**
 * construct_header - Build complete HTTP response header
 * @response_code: HTTP status code (200, 403, 404, 405)
 * @file_request: Path to file being sent (for Content-Type)
 *
 * Constructs the complete HTTP response header including the status line,
 * server name, Content-Type, and a blank line that indicates the header has
 * ended.
 *
 * Return: Allocated string containing header, or NULL on error
 */
char *construct_header(int response_code, const char *file_request) {
  static const char *server_name = "Server: Cyllenian\r\n";

  /*
   * Most headers are around 100 bytes, so we're being overly cautious in
   * allocating 1KB.
   */
  char *header = malloc(MAX_HEADER);
  if (!header) {
    log_event(ERROR, "Failed to allocate memory for header.");
    return NULL;
  }

  char response_code_msg[MAX_RESPONSE_CODE];
  if (get_response_code_msg(response_code_msg, response_code) == -1) {
    free(header);
    return NULL;
  };

  /*
   * Track remaining space in header buffer.
   *
   * We decrement this as we add each header line to ensure that we're not
   * overflowing the buffer.
   */
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

  /*
   * Append Server header.
   *
   * strncat() appends server_name to header, ensuring we don't
   * exceed MAX_HEADER total length.
   */
  strncat(header, server_name, strlen(server_name));

  /*
   * Determine Content-Type based on file extension.
   *
   * Note: get_content_type() includes the final \r\n\r\n that marks
   * the end of headers in its output.
   */
  char content_type[MAX_CONTENT_TYPE];
  get_content_type(content_type, file_request);

  const size_t content_type_length = strlen(content_type);

  /*
   * Append Content-Type header if there's space, making sure we have room for
   * the null terminator that strncat appends.
   */
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

/**
 * isolate_file_request - Extract file path from HTTP request
 * @request_buffer: Complete HTTP request from client
 *
 * Parses the HTTP request line to extract the requested file path.
 *
 * Return: Pointer to extracted path, or NULL if invalid format
 */
static char *isolate_file_request(char *request_buffer) {
  if (!request_buffer) {
    log_event(ERROR, "NULL request_buffer was passed to isolate_file_request.");
    return NULL;
  }

  /*
   * We advance the pointer by METHOD_LENGTH, which is set to five because that
   * is the length of the longest method we support, to reach the path to the
   * requested file.
   */
  char *file_request = request_buffer + METHOD_LENGTH;

  /*
   * The next space is where the file path ends, so we terminate the string
   * there.
   */
  char *space_position = strchr(file_request, ' ');
  if (space_position) {
    *space_position = '\0';
  }

  /*
   * We don't allow a trailing slash to avoid ambiguity over whether the user is
   * requesting a file or a directory (which we don't support).
   */
  if (file_request[strlen(file_request) - 1] == '/') {
    return NULL;
  }

  return file_request;
}

/**
 * get_method - Extract and validate HTTP method from request
 * @request_buffer: Complete HTTP request from client
 *
 * Return: Pointer to method string ("GET" or "HEAD"), or NULL if unsupported
 */
static const char *get_method(const char *request_buffer) {
  /*
   * Check if the request begins with GET
   */
  if (strncmp(request_buffer, "GET", 3) == 0) {
    return "GET";
  }

  /*
   * HEAD is like GET but we only send the headers, not the file body.
   */
  if (strncmp(request_buffer, "HEAD", 4) == 0) {
    return "HEAD";
  }

  /*
   * Unsupported method, we'll be sending 405.
   */
  return NULL;
}

/**
 * handle_error_case - Replace requested path with error page
 * @file_request: Path buffer to update
 * @error_page: Error page filename (403.html, 404.html, 405.html)
 *
 * When a request fails validation, we serve an error page instead of the
 * requested file. This function updates the path to point to the appropriate
 * error page.
 *
 * ERROR PAGE LOCATIONS:
 * Primary: ~/.local/share/cyllenian/website/404.html
 * Fallback: /etc/cyllenian/website/404.html
 *
 * Return: 0 on success, -1 on memory allocation failure
 */
static int handle_error_case(char **file_request, char *error_page) {
  /*
   * System-wide fallback location, this directory is created when running make
   * install.
   */
  static const char *fallback_website_path = "/etc/cyllenian/website";

  /*
   * Since we are sending an error page, we no longer need the original request.
   */
  free(*file_request);

  *file_request = malloc(PATH_MAX);
  if (!*file_request) {
    char malloc_fail_msg[LOG_MSG_MAX];
    snprintf(malloc_fail_msg, LOG_MSG_MAX,
             "Memory allocation failed for file request: %s", strerror(errno));
    log_event(ERROR, malloc_fail_msg);
    return -1;
  }

  /*
   * This sets file_request to be ~/.local/share/cyllenian/website/, which we
   * then append the error page to.
   */
  if (prepend_program_data_path(file_request, "website/") == -1) {
    return -1;
  }

  strcat(*file_request, error_page);

  /*
   * Check if the user has put a custom error page in their website directory
   * for this particular error. If not, use the one in the etc dir.
   */
  if (!file_exists(*file_request)) {
    free(*file_request);
    *file_request = malloc(PATH_MAX);
    if (!*file_request) {
      char malloc_fail_msg[LOG_MSG_MAX];
      snprintf(malloc_fail_msg, LOG_MSG_MAX,
               "Memory allocation failed for file request: %s",
               strerror(errno));
      log_event(ERROR, malloc_fail_msg);
      return -1;
    }

    /*
     * We zero out the buffer entirely before saving the path to it to ensure
     * that no data from previous writes remains.
     */
    memset(*file_request, 0, PATH_MAX);

    snprintf(*file_request, PATH_MAX, "%s/%s", fallback_website_path,
             error_page);
  }
  return 0;
}

/**
 * determine_response_code - Validate request and determine HTTP status
 * @request_buffer: Complete HTTP request from client
 * @file_request: Path to requested file (may be modified to error page)
 * @response_code: Output parameter for HTTP status code
 *
 * This is the core validation function. It performs several security checks
 * and determines what to send back to the client:
 *
 * Return: 0 on success (even if returning error code), -1 on fatal error
 */
int determine_response_code(const char *request_buffer, char **file_request,
                            int *response_code) {
  const char *method = get_method(request_buffer);
  if (!method) {
    /*
     * 405 Method Not Allowed.
     */
    *response_code = 405;
    return handle_error_case(file_request, "405.html");
  }

  /*
   * Remove consecutive slashes, trailing slash, and ensure null termination of
   * request string.
   */
  normalize_request_path(*file_request);

  /*
   * Directory traversal attacks attempt to access files outside the
   * website directory by using ../ in the path.
   *
   * If detected, return 403 Forbidden instead of allowing the request.
   */
  if (contains_traversal_patterns(*file_request)) {
    *response_code = 403;
    return handle_error_case(file_request, "403.html");
  }

  if (!file_exists(*file_request)) {
    /*
     * File not found, return 404. This is the most common error to see due to
     * mistyping or the user having bookmarked a page that has been moved.
     */
    *response_code = 404;
    return handle_error_case(file_request, "404.html");
  }

  /*
   * All good, 200 OK
   */
  *response_code = 200;

  return 0;
}

/**
 * get_requested_file_path - Build full filesystem path from HTTP request
 * @path_buffer: Output buffer for full path
 * @request_buffer: Complete HTTP request from client
 *
 * Converts the HTTP request path into a full filesystem path by extracting the
 * path from the request and prepending the website directory to it.
 *
 * Return: 0 on success, -1 on failure
 */
int get_requested_file_path(char **path_buffer, char *request_buffer) {
  /*
   * Duplicate request buffer so we can modify it without affecting the
   * original. strdup() allocates new memory, so we'll need to free it when
   * we're done using it.
   */
  char *file_request_buffer = strdup(request_buffer);
  if (!file_request_buffer) {
    log_event(ERROR, "Failed to duplicate file_request");
    return -1;
  }

  char *file_request = isolate_file_request(file_request_buffer);
  if (!file_request) {
    /*
     * Malformed requests result in 404 rather than a crash.
     */
    file_request = "404.html";
  }

  /*
   * This gives ~/.local/share/cyllenian/website/, which we'll then append the
   * requested path to.
   */
  if (prepend_program_data_path(path_buffer, "website/") == -1) {
    return -1;
  }

  strcat(*path_buffer, file_request);

  free(file_request_buffer);

  return 0;
}
