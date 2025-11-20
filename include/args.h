/**
 * args.h
 *
 * Function for parsing CLI arguments via getopt, the POSIX standard for doing
 * so.
 */

#ifndef ARGS_H
#define ARGS_H

/*
 * Return: 0 if program should exit successfully, 1 if server
 * initialization should continue, or -1 on error
 */
int process_args(int argc, char *argv[]);

#endif
