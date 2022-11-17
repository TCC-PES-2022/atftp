// Modified by Kollins G. Lima <kgl2@cin.ufpe.br>
/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftp.c
 *    main client file.
 *
 * $Id: tftp.c,v 1.47 2004/03/15 23:55:56 jp Exp $
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
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <signal.h>

#include "tftp.h"
#include "tftp_io.h"
#include "tftp_def.h"
#include "atftp_logger.h"
#include "options.h"

#include "tftp_api.h"
#define MAX_PARAM_SIZE 32
/*
 * We're going to store everything as a string because
 * the original project reads from argv. So we don't
 * have to change the inner code.
 */
struct TftpHandler
{
     char host[MAX_PARAM_SIZE];
     char port[MAX_PARAM_SIZE];

     /* Structure to hold some information that must be passed to
      * functions.
      */
     struct client_data data;
};

/* defined as extern in tftp_file.c and mtftp_file.c, set by the signal
   handler */
int tftp_cancel = 0;
int tftp_prevent_sas = 0;

/* local flags */
int interactive = 1;  /* if false, we run in batch mode */
int tftp_result = OK; /* status of tftp_send_file or
                         tftp_receive_file, used for status() */

/* tftp.c local only functions. */
int tftp_cmd_line_options(int argc, char **argv);

/* Functions associated with the tftp commands. */
int set_peer(int argc, char **argv, TftpHandlerPtr);
int set_mode(int argc, char **argv, TftpHandlerPtr);
int set_option(int argc, char **argv, TftpHandlerPtr);
int put_file(int argc, char **argv, TftpHandlerPtr);
int get_file(int argc, char **argv, TftpHandlerPtr);

TftpOperationResult create_tftp_handler(TftpHandlerPtr *handler)
{
     (*handler) = malloc(sizeof(struct TftpHandler));

     (*handler)->host[0] = '\0';
     (*handler)->port[0] = '\0';

     (*handler)->data.data_buffer = NULL;
     (*handler)->data.tftp_options = NULL;
     (*handler)->data.tftp_options_reply = NULL;
     (*handler)->data.tftp_error_cb = NULL;
     (*handler)->data.tftp_error_ctx = NULL;
     (*handler)->data.tftp_fetch_data_received_cbk = NULL;
     (*handler)->data.tftp_fetch_data_received_ctx = NULL;

     return TFTP_OK;
}

TftpOperationResult destroy_tftp_handler(TftpHandlerPtr *handler)
{
     if ((*handler)->data.data_buffer != NULL)
     {
          free((*handler)->data.data_buffer);
     }

     if ((*handler)->data.tftp_options != NULL)
     {
          free((*handler)->data.tftp_options);
     }

     if ((*handler)->data.tftp_options_reply != NULL)
     {
          free((*handler)->data.tftp_options_reply);
     }

     if ((*handler) == NULL)
     {
          return TFTP_ERROR;
     }
     free((*handler));
     return TFTP_OK;
}

TftpOperationResult register_tftp_error_callback(
    const TftpHandlerPtr handler,
    tftp_error_callback callback,
    void *context)
{
     handler->data.tftp_error_cb = callback;
     handler->data.tftp_error_ctx = context;
     return TFTP_OK;
}

TftpOperationResult register_tftp_fetch_data_received_callback(
    const TftpHandlerPtr handler,
    tftp_fetch_data_received_callback callback,
    void *context)
{
     handler->data.tftp_fetch_data_received_cbk = callback;
     handler->data.tftp_fetch_data_received_ctx = context;
     return TFTP_OK;
}

TftpOperationResult set_connection(
    TftpHandlerPtr handler,
    const char *host,
    const int port)
{
     if (handler == NULL)
     {
          return TFTP_ERROR;
     }
     if (host == NULL)
     {
          return TFTP_ERROR;
     }
     if (port < 0)
     {
          return TFTP_ERROR;
     }
     strncpy(handler->host, host, MAX_PARAM_SIZE);
     sprintf(handler->port, "%d", port);
     return TFTP_OK;
}

TftpOperationResult send_file(
    TftpHandlerPtr handler,
    const char *filename,
    FILE *fp)
{
     if (handler == NULL)
     {
          return TFTP_ERROR;
     }
     if (filename == NULL)
     {
          return TFTP_ERROR;
     }
     if (fp == NULL)
     {
          return TFTP_ERROR;
     }

     handler->data.fp = fp;
     char *tmp_filename = strdup(filename);
     int ret = put_file(2,
                        (char *[]){
                            "put",
                            tmp_filename},
                        handler);
     free(tmp_filename);

     return ret == OK ? TFTP_OK : TFTP_ERROR;
}

TftpOperationResult fetch_file(
    TftpHandlerPtr handler,
    const char *filename,
    FILE *fp)
{
     if (handler == NULL)
     {
          return TFTP_ERROR;
     }
     if (filename == NULL)
     {
          return TFTP_ERROR;
     }
     if (fp == NULL)
     {
          return TFTP_ERROR;
     }

     handler->data.fp = fp;
     char *tmp_filename = strdup(filename);
     int ret = get_file(2,
                        (char *[]){
                            "get",
                            tmp_filename},
                        handler);
     free(tmp_filename);

     return ret == OK ? TFTP_OK : TFTP_ERROR;
}

/*
 * Init default configuration and open the socket
 */
TftpOperationResult config_tftp(
    const TftpHandlerPtr handler)
{
     /* Allocate memory for data buffer. */
     if ((handler->data.data_buffer = malloc((size_t)SEGSIZE + 4)) == NULL)
     {
          fprintf(stderr, "tftp: memory allcoation failed.\n");
          return TFTP_ERROR;
     }
     handler->data.data_buffer_size = SEGSIZE + 4;

     /* Allocate memory for tftp option structure. */
     if ((handler->data.tftp_options =
              malloc(sizeof(tftp_default_options))) == NULL)
     {
          fprintf(stderr, "tftp: memory allocation failed.\n");
          return TFTP_ERROR;
     }
     /* Copy default options. */
     memcpy(handler->data.tftp_options, tftp_default_options,
            sizeof(tftp_default_options));

     /* Allocate memory for tftp option reply from server. */
     if ((handler->data.tftp_options_reply =
              malloc(sizeof(tftp_default_options))) == NULL)
     {
          fprintf(stderr, "tftp: memory allocation failed.\n");
          return TFTP_ERROR;
     }
     /* Copy default options. */
     memcpy(handler->data.tftp_options_reply, tftp_default_options,
            sizeof(tftp_default_options));

     handler->data.timeout = TIMEOUT;
     handler->data.checkport = 1;
     handler->data.trace = 1;
     handler->data.verbose = 1;

     int config_ret = OK;

     // Set connection parameters
     char *set_peer_argv[] = {
         "set_peer",
         handler->host,
         handler->port};
     config_ret |= set_peer(3, set_peer_argv, handler);

     // Set mode
     char *set_mode_argv[] = {
         "option",
         "octet"};
     config_ret |= set_mode(2, set_mode_argv, handler);

     //     Set options
     char *set_blksize_option_argv[] = {
         "option",
         "blksize",
         "512"};
     config_ret |= set_option(3, set_blksize_option_argv, handler);

     return config_ret == OK ? TFTP_OK : TFTP_ERROR;
}

/*
 * Update sa_peer structure, hostname and port number adequately
 */
int set_peer(int argc, char **argv, TftpHandlerPtr handler)
{
     struct addrinfo hints, *addrinfo;
     int err;

     /* sanity check */
     if ((argc < 2) || (argc > 3))
     {
          fprintf(stderr, "Usage: %s host-name [port]\n", argv[0]);
          return ERR;
     }

     /* look up the service and host */
     memset(&hints, 0, sizeof(hints));
     hints.ai_socktype = SOCK_DGRAM;
     hints.ai_flags = AI_CANONNAME;
     err = getaddrinfo(argv[1], argc == 3 ? argv[2] : "tftp",
                       &hints, &addrinfo);
     /* if valid, update s_inn structure */
     if (err == 0)
          err = sockaddr_set_addrinfo(&handler->data.sa_peer, addrinfo);
     if (err == 0)
     {
          Strncpy(handler->data.hostname, addrinfo->ai_canonname,
                  sizeof(handler->data.hostname));
          handler->data.hostname[sizeof(handler->data.hostname) - 1] = 0;
          freeaddrinfo(addrinfo);
     }
     else
     {
          if (err == EAI_SERVICE)
          {
               if (argc == 3)
                    fprintf(stderr, "%s: bad port number.\n", argv[2]);
               else
                    fprintf(stderr, "tftp: udp/tftp, unknown service.\n");
          }
          else
          {
               fprintf(stderr, "tftp: unknown host %s.\n", argv[1]);
          }
          handler->data.connected = 0;
          return ERR;
     }
     /* copy port number to data structure */
     handler->data.port = sockaddr_get_port(&handler->data.sa_peer);

     handler->data.connected = 1;
     return OK;
}

/*
 * Set the mode to netascii or octet
 */
int set_mode(int argc, char **argv, TftpHandlerPtr handler)
{
     if (argc > 2)
     {
          fprintf(stderr, "Usage: %s [netascii] [octet]\n", argv[0]);
          return ERR;
     }
     if (argc == 1)
     {
          fprintf(stderr, "Current mode is %s.\n",
                  handler->data.tftp_options[OPT_MODE].value);
          return OK;
     }
     if (strcasecmp("netascii", argv[1]) == 0)
          Strncpy(handler->data.tftp_options[OPT_MODE].value, "netascii",
                  VAL_SIZE);
     else if (strcasecmp("octet", argv[1]) == 0)
          Strncpy(handler->data.tftp_options[OPT_MODE].value, "octet",
                  VAL_SIZE);
     else
     {
          fprintf(stderr, "tftp: unkowned mode %s.\n", argv[1]);
          fprintf(stderr, "Usage: %s [netascii] [octet]\n", argv[0]);
          return ERR;
     }
     return OK;
}

/*
 * Set an option
 */
int set_option(int argc, char **argv, TftpHandlerPtr handler)
{
     char value[VAL_SIZE];

     /* if there are no arguments */
     if ((argc < 2) || (argc > 3))
     {
          fprintf(stderr, "Usage: option <option name> [option value]\n");
          fprintf(stderr, "       option disable <option name>\n");
          if (handler->data.tftp_options[OPT_BLKSIZE].specified)
               fprintf(stderr, "  blksize:   %s\n",
                       handler->data.tftp_options[OPT_BLKSIZE].value);
          else
               fprintf(stderr, "  blksize:   disabled\n");
          return ERR;
     }

     if (argc == 3)
     {
          if (opt_set_options(handler->data.tftp_options, argv[1], argv[2]) == ERR)
          {
               fprintf(stderr, "no option named %s\n", argv[1]);
               return ERR;
          }
     }
     /* print the new value for that option */
     if (opt_get_options(handler->data.tftp_options, argv[1], value) == ERR)
          fprintf(stderr, "Option %s disabled\n", argv[1]);
     else
          fprintf(stderr, "Option %s = %s\n", argv[1], value);
     return OK;
}

/*
 * Put a file to the server.
 */
int put_file(int argc, char **argv, TftpHandlerPtr handler)
{
     socklen_t len = sizeof(handler->data.sa_local);

     if ((argc < 2) || (argc > 3))
     {
          fprintf(stderr, "Usage: put local_file [remote_file]\n");
          return ERR;
     }
     if (!handler->data.connected)
     {
          fprintf(stderr, "Not connected.\n");
          return ERR;
     }
     if (argc == 2)
     {
          Strncpy(handler->data.local_file, argv[1], VAL_SIZE);
          Strncpy(handler->data.tftp_options[OPT_FILENAME].value, argv[1], VAL_SIZE);
     }
     else
     {
          Strncpy(handler->data.local_file, argv[1], VAL_SIZE);
          Strncpy(handler->data.tftp_options[OPT_FILENAME].value, argv[2], VAL_SIZE);
     }

     /* open a UDP socket */
     handler->data.sockfd = socket(handler->data.sa_peer.ss_family, SOCK_DGRAM, 0);
     if (handler->data.sockfd < 0)
     {
          perror("tftp: ");
          exit(ERR);
     }
     memset(&handler->data.sa_local, 0, sizeof(handler->data.sa_local));
     bind(handler->data.sockfd, (struct sockaddr *)&handler->data.sa_local,
          sizeof(handler->data.sa_local));
     getsockname(handler->data.sockfd, (struct sockaddr *)&handler->data.sa_local, &len);

     /* do the transfer */
     gettimeofday(&handler->data.start_time, NULL);
     tftp_result = tftp_send_file(&handler->data);
     gettimeofday(&handler->data.end_time, NULL);

     /* close the socket */
     fsync(handler->data.sockfd);
     close(handler->data.sockfd);

     return tftp_result;
}

/*
 * Receive a file from the server.
 */
int get_file(int argc, char **argv, TftpHandlerPtr handler)
{

     char *tmp;
     socklen_t len = sizeof(handler->data.sa_local);

     if ((argc < 2) || (argc > 3))
     {
          fprintf(stderr, "Usage: %s remote_file [local_file]\n", argv[0]);
          return ERR;
     }
     if (!handler->data.connected)
     {
          fprintf(stderr, "Not connected.\n");
          return ERR;
     }
     if (argc == 2)
     {
          Strncpy(handler->data.tftp_options[OPT_FILENAME].value,
                  argv[1], VAL_SIZE);
          tmp = strrchr(argv[1], '/');
          if (tmp < argv[1])
               tmp = argv[1];
          else
               tmp++;
          Strncpy(handler->data.local_file, tmp, VAL_SIZE);
     }
     else
     {
          Strncpy(handler->data.local_file, argv[2], VAL_SIZE);
          Strncpy(handler->data.tftp_options[OPT_FILENAME].value, argv[1], VAL_SIZE);
     }

     /* open a UDP socket */
     handler->data.sockfd = socket(handler->data.sa_peer.ss_family, SOCK_DGRAM, 0);
     if (handler->data.sockfd < 0)
     {
          perror("tftp: ");
          exit(ERR);
     }
     memset(&handler->data.sa_local, 0, sizeof(handler->data.sa_local));
     bind(handler->data.sockfd, (struct sockaddr *)&handler->data.sa_local,
          sizeof(handler->data.sa_local));
     getsockname(handler->data.sockfd, (struct sockaddr *)&handler->data.sa_local, &len);

     /* do the transfer */
     gettimeofday(&handler->data.start_time, NULL);
     tftp_result = tftp_receive_file(&handler->data);

     gettimeofday(&handler->data.end_time, NULL);

     /* close the socket */
     fsync(handler->data.sockfd);
     close(handler->data.sockfd);

     return tftp_result;
}