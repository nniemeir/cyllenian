/**
 * paths_security.h
 *
 * Responsible for validating that the file paths being requested are inside the
 * website directory, ESSENTIAL for security.
 */

#ifndef PATH_SECURITY_H
#define PATH_SECURITY_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Checks if path contains any common directory traversal patterns that could
 * allow accessing files outside the website directory.
 *
 * Directory traversal attacks are an elementary tactic to try to move upwards
 * in the directory structure to access files outside of the website directory.
 *
 * Return: true if traversal pattern detected, false if safe
 */
bool contains_traversal_patterns(const char *file_request);

/**
 * Cleans up path for security checks by removing trailing slashes, consecutive
 * slashes, and null terminating.
 */
void normalize_request_path(char *file_request);

#endif
