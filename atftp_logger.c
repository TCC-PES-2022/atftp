// Modified by Kollins G. Lima <kgl2@cin.ufpe.br>
/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * atftp_logger.c
 *    functions for logging messages.
 *
 * $Id: atftp_logger.c,v 1.12 2004/02/27 02:05:26 jp Exp $
 *
 * Copyright (c) 2000 Jean-Pierre Lefebvre <helix@step.polymtl.ca>
 *                and Remi Lefebvre <remi@debian.org>
 *
 * atftp is free software; you can redistribute them and/or modify them
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include "atftp_logger.h"

#define MAXLEN 128

static int log_priority = 0;
static char *log_ident;
static FILE *log_fp = NULL;

/*
 * Open a file for logging. If filename is NULL, then use
 * stderr for the client or the syslog for the server. Log
 * only message less or equal to priority.
 */
void open_atftp_logger(char *ident, char *filename, int priority)
{
     close_atftp_logger(); /* make sure we initialise variables and close
                        previously opened log. */

     log_priority = priority;
}

/*
 * Same as syslog but allows one to format a string, like printf, when logging to
 * file. This fonction will either call syslog or fprintf depending of the
 * previous call to open_atftp_logger().
 */
void atftp_logger(int severity, const char *fmt, ...)
{
     char message[MAXLEN];
     char time_buf[MAXLEN];
     char hostname[MAXLEN];
     time_t t;
     struct tm *tm;

     va_list args;
     va_start(args, fmt);

     time(&t);
     tm = localtime(&t);
     strftime(time_buf, MAXLEN, "%b %d %H:%M:%S", tm);
     gethostname(hostname, MAXLEN);

     if (severity <= log_priority)
     {
          vsnprintf(message, sizeof(message), fmt, args);

          if (log_fp)
          {
               fprintf(log_fp, "%s %s %s[%d.%li]: %s\n", time_buf, hostname,
                       log_ident, getpid(), pthread_self(), message);
               fflush(log_fp);
          }
          else
               fprintf(stderr, "%s %s %s[%d.%li]: %s\n", time_buf, hostname,
                       log_ident, getpid(), pthread_self(), message);
     }
     va_end(args);
}

/*
 * Close the file or syslog. Initialise variables.
 */
void close_atftp_logger(void)
{
     log_priority = 0;
}
