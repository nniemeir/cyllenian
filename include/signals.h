/**
 * signals.h
 *
 * Signal handling interface.
 */

#ifndef SIGNALS_H
#define SIGNALS_H

/**
 * Registers custom SIGINT handler to allow clean shutdown when user
 * presses Ctrl+C. SIGINT terminates the process immediately by default, which
 * would prevent proper cleanup if we didn't override it's behavior.
 *
 * Return: 0 on success, -1 on failure
 */
int sig_handler_init(void);

#endif
