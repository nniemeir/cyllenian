/**
 * response.h
 *
 * Responsible for processing HTTP response.
 */

#ifndef RESPONSE_H
#define RESPONSE_H

/**
 * Maximum size for request/response buffers, prevents large file requests from
 * consuming excessive memory.
 */
#define BUFFER_SIZE 1048576

/**
 * Maximum HTTP response header size, you are unlikely to come across headers
 * this large.
 */
#define MAX_HEADER 1024

/**
 * Maximum Content-Type line size, this is more than enough room for all
 * supported MIME types.
 */
#define MAX_CONTENT_TYPE 128

/**
 * Maximum response code line size
 */
#define MAX_RESPONSE_CODE 128

/**
 * Size to skip past HTTP method in request to reach the
 * requested file path.
 */
#define METHOD_LENGTH 5

/**
 * Number of MIME type mappings. Used for array bounds checking and binary
 * search of the mime_type_associations array, so it must be updated if
 * additional types are added later.
 */
#define NUM_OF_MIME_TYPES 14

/**
 * Number of supported HTTP status codes, must be updated if additional response
 * codes are added to response_code_associations array.
 */
#define NUM_OF_RESPONSE_CODES 5

/*
 * Used to map file extensions to Content-Type headers.
 * Array of these structs must be kept sorted alphabetically by extension
 * for binary search to work.
 */
struct mime_type {
  char extension[6];
  char mime_type[50];
};

/**
 * Used to map numeric status codes to full HTTP status lines for response
 * headers.
 */
struct response_code {
  int code;
  char message[50];
};

/**
 * Build complete HTTP response header.
 *
 * Return: Pointer to allocated header string, or NULL on error
 */
char *construct_header(int response_code, const char *file_request);

/**
 * Performs validation checks and determines the appropriate HTTP status code:
 *
 * If validation fails, file_request is updated to point to appropriate
 * error page (403.html, 404.html, 405.html).
 *
 * Return: 0 on success (even if returning error code), -1 on fatal error
 */
int determine_response_code(const char *request_buffer, char **file_request,
                            int *response_code);

/**
 * Parses HTTP request to extract requested path, then constructs full
 * filesystem path by prepending website directory.
 *
 * Return: 0 on success, -1 on error
 */
int get_requested_file_path(char **path_buffer, char *request_buffer);

/**
 * Determines MIME type based on file extension and constructs Content-Type
 * header with proper formatting.
 *
 * The CRLF is added to the end of this line since Content-Type
 * is always our last header.
 */
void get_content_type(char content_type[MAX_CONTENT_TYPE],
                      const char *file_request);

#endif
