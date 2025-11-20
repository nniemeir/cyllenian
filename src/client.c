/**
 * client.c
 *
 * Client request handling for HTTP/HTTPS connections.
 *
 * OVERVIEW:
 * Responsible for the request-response cycle for each individual client.
 * Each function is responsible for one stage of processing:
 *
 * ERROR HANDLING:
 * If any step fails, we call server_cleanup() and return early. This
 * ensures SSL structures and file descriptors are properly freed regardless of
 * when the error occurs.
 */

#include <unistd.h>

#include "file.h"
#include "log.h"
#include "response.h"
#include "server.h"

/**
 * read_from_client - Read HTTP request from client over SSL connection
 * @server: Server context containing SSL connection
 * @request_buffer: Buffer to store the incoming request
 *
 * Reads data from the encrypted SSL connection into our buffer.
 *
 * BUFFER_SIZE (1MB) is adequate for our purposes, as we do not support the POST
 * method which would allow the client to send a file in their request.
 *
 * Return: 0 on success, -1 on failure
 */
static int read_from_client(struct server_ctx *server, char *request_buffer) {
  /*
   * Read up to (BUFFER_SIZE - 1) bytes from the SSL connection so that we leave
   * room for the null terminator since we'll be parsing the request as a
   * string.
   *
   * SSL_read() may return fewer bytes than requested:
   * - If request is smaller than buffer
   * - If network packet boundaries split the data
   * - If TLS record boundaries split the data
   *
   * Most HTTP requests are < 8KB, so a single call is sufficient.
   */
  int bytes_read = SSL_read(server->ssl, request_buffer, BUFFER_SIZE - 1);

  if (bytes_read <= 0) {
    log_event(ERROR, "Failed to read from connection.");
    server_cleanup();
    return -1;
  }

  request_buffer[bytes_read] = '\0';

  return 0;
}

/**
 * process_request - Parse request and determine appropriate response
 * @path_buffer: Output parameter for path to response file
 * @request_buffer: The HTTP request from client
 * @response_code: Output parameter for HTTP status code (200, 404, etc.)
 *
 * This function analyzes the HTTP request and decides what should be sent as a
 * response.
 *
 * Return: 0 on success, -1 on error
 */
static int process_request(char **path_buffer, char *request_buffer,
                           int *response_code) {
  /*
   * Extract the requested file path from the HTTP request and prepend the
   * website directory to it.
   *
   * path_buffer is where we will look for the file to send.
   */
  if (get_requested_file_path(path_buffer, request_buffer) == -1) {
    log_event(FATAL, "Failed to get requested file path.");
    server_cleanup();
    return -1;
  }

  /*
   * Analyze the request and path to determine HTTP status code.
   *
   * VALIDATION CHECKS PERFORMED:
   * 1. Is the HTTP method supported? (405 Method Not Allowed)
   * 2. Does path contain directory traversal attempts? (403 Forbidden)
   * 3. Does the file exist? (404 Not Found)
   *
   * If validation fails, path_buffer is updated to point to the
   * appropriate error page.
   *
   * ERROR PAGE LOCATIONS:
   * Primary: ~/.local/share/cyllenian/website/404.html
   * Fallback: /etc/cyllenian/website/404.html
   *
   * The fallback ensures error pages work even if user hasn't installed
   * custom ones in their website directory.
   */
  if (determine_response_code(request_buffer, path_buffer, response_code) ==
      -1) {
    log_event(FATAL, "Failed to determine response code.");
    server_cleanup();
    return -1;
  }
  return 0;
}

/**
 * write_to_client - Send HTTP response to client over SSL connection
 * @ssl: SSL connection to write to
 * @header: HTTP response header (status, content-type, etc.)
 * @path_buffer: Path to file to send in response body
 * @file_size: Output parameter for size of file sent (for logging)
 *
 * Sends the complete HTTP response in two parts:
 * 1. Header (HTTP status, metadata)
 * 2. Body (file contents)
 *
 * HTTP RESPONSE FORMAT:
 *   HTTP/1.1 200 OK\r\n
 *   Server: Cyllenian\r\n
 *   Content-Type: text/html\r\n
 *   \r\n
 *   <html>...</html>
 *
 * HTTP responses use two Carriage Return-Line Feeds (\r\n\r\n) to separate the
 * header from the body. CRLF moves the cursor to the beginning of the line (\r)
 * then down one line (\n). Use of CRLF is common among older network protocols
 * as they were optimizing for printing on teletypes.
 *
 * Return: 0 on success, -1 on failure
 */
static int write_to_client(SSL *ssl, char **header, char **path_buffer,
                           size_t *file_size) {
  /*
   * Send the HTTP response header over SSL.
   *
   * RETURN VALUES:
   * > 0 : Number of bytes written
   * <= 0 : Error occurred
   *
   * COMMON ERRORS:
   * - Client closed connection before we could respond
   * - Network timeout
   * - SSL encryption error
   */
  if (SSL_write(ssl, *header, strlen(*header)) <= 0) {
    log_event(ERROR, "Failed to write header to connection.");
    free(header);
    server_cleanup();
    return -1;
  }

  /*
   * Read entire file into memory like caveman. For large files (videos,
   * large downloads), a production server would use:
   * - Chunked transfer encoding
   * - Streaming (read and send in chunks)
   * - sendfile() system call (zero-copy transfer)
   *
   * file_size is set to the number of bytes read, we'll need this to know how
   * many bytes to write to the connection.
   */
  unsigned char *response_file = read_file(*path_buffer, file_size);

  free(*path_buffer);

  /*
   * Send the file contents as the HTTP response body.
   *
   * The Content-Type field we include in the header tells the browser how to
   * intrepret these bytes (e.g, as an HTML file or an image).
   */
  if (SSL_write(ssl, response_file, *file_size) <= 0) {
    log_event(ERROR, "Failed to write file to connection.");
    free(response_file);
    server_cleanup();
    return -1;
  }

  free(response_file);
  return 0;
}

/**
 * handle_client - Complete request-response cycle for one client
 * @server: Server context (SSL connection, socket)
 * @clientfd: File descriptor for client's TCP connection
 *
 * This is the main entry point for handling a client request.
 *
 * 1. SSL handshake (establish encrypted connection)
 * 2. Read HTTP request
 * 3. Process request (find file, validate, determine status)
 * 4. Send HTTP response
 * 5. Close connection and cleanup
 *
 * FORKING CONTEXT:
 * This function runs in a forked child process. When it returns, the child
 * process exits, which:
 * - Closes all open file descriptors
 * - Frees all allocated memory
 * - Removes process from system
 *
 * Memory leaks here wouldn't be the end of the world, but we free explicitly
 * here anyway for clarity.
 *
 * ERROR HANDLING:
 * If any step fails, we return immediately. The child process exits and
 * the parent continues accepting new connections. Failed requests DO NOT stop
 * the server from continuing.
 */
void handle_client(struct server_ctx *server, int clientfd) {
  /*
   * Performs the TLS handshake with the client:
   * - Client verifies our certificate
   * - Both agree on encryption algorithm
   * - Both derive shared encryption keys
   *
   * After this succeeds, all further communication is encrypted.
   *
   * If handshake fails, we return immediately. Common failure reasons:
   * - Client doesn't support our TLS version
   * - Certificate is invalid or expired
   * - Client closed connection during handshake
   */
  if (setup_ssl(clientfd) == -1) {
    return;
  }

  /*
   * Allocate a 1MB buffer for the incoming request. Most HTTP requests
   * are small, but large cookie strings and requests with many headers are not
   * unheard of.
   *
   * The buffer is stack-allocated (not malloc) because:
   * - Faster allocation
   * - Automatically freed when function returns
   * - We know the size at compile time
   */
  char request_buffer[BUFFER_SIZE];
  if (read_from_client(server, request_buffer) == -1) {
    return;
  }

  /*
   * PATH_MAX (4096 bytes) is the maximum path length on most Unix systems.
   * This buffer will hold the full path to the requested file.
   */
  char *path_buffer = malloc(PATH_MAX);
  if (!path_buffer) {
    log_event(ERROR, "Failed to allocate memory for path_buffer.");
    server_cleanup();
    return;
  }

  /*
   * Determine what file to send and what HTTP status code to use.
   *
   * This sets path_buffer to the path of the file to send and response_code to
   * the appropriate value.
   */
  int response_code;

  if (process_request(&path_buffer, request_buffer, &response_code) == -1) {
    free(path_buffer);
    return;
  }

  /*
   * Builds the header based on the response_code and path_buffer, the latter's
   * extension determines what Content-Type will be set to.
   */
  char *header = construct_header(response_code, path_buffer);
  if (!header) {
    log_event(ERROR, "Failed to construct header.");
    server_cleanup();
    return;
  }

  /*
   * Writes both header and file contents to the SSL connection.
   * file_size is set by write_to_client() for logging purposes.
   */
  size_t file_size;
  if (write_to_client(server->ssl, &header, &path_buffer, &file_size) == -1) {
    server_cleanup();
    free(header);
    return;
  }

  /*
   * Creates a log entry stating the date, time, log level, server hostname,
   * request method, request path, response code, and response size in bytes.
   */
  size_t response_size = strlen(header) + file_size;
  log_request(request_buffer, response_code, response_size);

  free(header);

  /*
   * Gracefully close the SSL connection
   *
   * SSL_shutdown() sends a "close_notify" alert to the client, which:
   * - Tells client we're done sending data
   * - Allows client to verify all data received
   * - Prevents truncation attacks
   *
   * RETURN VALUES:
   * 0: Shutdown in progress (need to call again)
   * 1: Shutdown complete
   * < 0: Error occurred
   *
   * We ignore the return value and error here because:
   * - Client might have already closed connection
   * - We're about to close everything anyway
   * - Failure to shutdown gracefully is not critical
   *
   * A production server might call SSL_shutdown() twice for complete
   * bidirectional shutdown, but for this simple server, once is enough.
   */
  if (SSL_shutdown(server->ssl) < 0) {
    log_event(ERROR, "Failed to shutdown connection.");
  }

  server_cleanup();

  /*
   * Closes the TCP connection to the client. After this:
   * - Client can no longer send/receive data
   * - File descriptor is released back to OS
   * - Client's TCP connection enters TIME_WAIT state
   */
  close(clientfd);
}
