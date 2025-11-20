/**
 * log.h
 *
 * Logging interface that outputs data in a similar format to nginx.
 */
#ifndef LOG_H
#define LOG_H

/**
 * Maximum length of log message, longer messages are automatically truncated.
 */
#define LOG_MSG_MAX 1024

/**
 * Used in buffer size calculations to reserve space for '\0'.
 */
#define NULL_TERMINATOR_LENGTH 1

/**
 * Defines severity levels for log messages with higher values indicating
 * more severe issues. DEBUG and INFO are sent to stdout, while the others go to
 * stderror.
 */
enum levels { DEBUG, INFO, WARN, ERROR, FATAL };

/**
 * Log a message with timestamp and level
 * @log_level: Severity level (DEBUG, INFO, WARN, ERROR, FATAL)
 * @msg: Message string to log
 *
 * Main logging function. Formats message with timestamp and level, outputs
 * to console and optionally to file.
 *
 * FORMAT:
 * [MM/DD/YYYY HH:MM:SS] LEVEL  Message
 *
 * CONSTRAINTS:
 * - msg must not be NULL
 * - msg must not be empty
 * - msg should be < LOG_MSG_MAX bytes, will be truncated otherwise
 *
 * Returns early and prints a warning on error, this function failing is not
 * serious enough to warrant shutting down the program.
 */
void log_event(int log_level, const char *msg);

/**
 * Log HTTP request in Common Log Format
 *
 * USAGE:
 * Called after successfully sending response to client.
 * Provides access log for traffic analysis.
 */
void log_request(const char *request_buffer, int response_code,
                 int response_size);

#endif
