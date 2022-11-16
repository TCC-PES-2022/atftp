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

#undef HAVE_MTFTP

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
     char tftp_options[OPT_NUMBER][VAL_SIZE];

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
#ifdef HAVE_MTFTP
int mtftp_opt(int argc, char **argv, TftpHandlerPtr);
#endif

int set_verbose(int argc, char **argv, TftpHandlerPtr);
int set_trace(int argc, char **argv, TftpHandlerPtr);
int status(int argc, char **argv, TftpHandlerPtr);
int set_timeout(int argc, char **argv, TftpHandlerPtr);

/* All supported commands. */
#if 0
struct command {
     const char *name;
     int (*func)(int argc, char **argv);
     const char *helpmsg;
} cmdtab[] = {
     {"connect", set_peer, "connect to tftp server"},
     {"mode", set_mode, "set file transfer mode (netascii/octet)"},
     {"option", set_option, "set RFC1350 options"},
     {"put", put_file, "upload a file to the host"},
     {"get", get_file, "download a file from the host"},
#ifdef HAVE_MTFTP
     {"mtftp", mtftp_opt, "set mtftp variables"},
     {"mget", get_file, "download file from mtftp server"},
#endif
     {"quit", quit, "exit tftp"},
     {"verbose", set_verbose, "toggle verbose mode"},
     {"trace", set_trace, "toggle trace mode"},
     {"status", status, "print status information"},
     {"timeout", set_timeout, "set the timeout before a retry"},
     {"help", help, "print help message"},
     {"?", help, "print help message"},
     {NULL, NULL, NULL}
};
#endif

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
     (*handler)->data.tftp_option_accepted_cbk = NULL;
     (*handler)->data.tftp_option_accepted_ctx = NULL;

     memset((*handler)->tftp_options, 0, OPT_NUMBER * VAL_SIZE);

     /* Allocate memory for data buffer. */
     if (((*handler)->data.data_buffer = malloc((size_t)SEGSIZE + 4)) == NULL)
     {
          fprintf(stderr, "tftp: memory allcoation failed.\n");
          return TFTP_ERROR;
     }
     (*handler)->data.data_buffer_size = SEGSIZE + 4;

     /* Allocate memory for tftp option structure. */
     if (((*handler)->data.tftp_options =
              malloc(sizeof(tftp_default_options))) == NULL)
     {
          fprintf(stderr, "tftp: memory allocation failed.\n");
          return TFTP_ERROR;
     }
     /* Copy default options. */
     memcpy((*handler)->data.tftp_options, tftp_default_options,
            sizeof(tftp_default_options));

     /* Allocate memory for tftp option reply from server. */
     if (((*handler)->data.tftp_options_reply =
              malloc(sizeof(tftp_default_options))) == NULL)
     {
          fprintf(stderr, "tftp: memory allocation failed.\n");
          return TFTP_ERROR;
     }
     /* Copy default options. */
     memcpy((*handler)->data.tftp_options_reply, tftp_default_options,
            sizeof(tftp_default_options));

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

TftpOperationResult register_tftp_option_accepted_callback(
    const TftpHandlerPtr handler,
    tftp_option_accepted_callback callback,
    void *context)
{
     handler->data.tftp_option_accepted_cbk = callback;
     handler->data.tftp_option_accepted_ctx = context;
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

TftpOperationResult set_tftp_option(
    TftpHandlerPtr handler,
    TftpOption option,
    const char *value)
{
     if (handler == NULL)
     {
          return TFTP_ERROR;
     }
     if (value == NULL)
     {
          return TFTP_ERROR;
     }

     switch (option)
     {
     case TFTP_BLOCKSIZE_OPTION:
          strncpy(handler->tftp_options[OPT_BLKSIZE], value, VAL_SIZE);
          break;
     case TFTP_PORT_OPTION:
          strncpy(handler->tftp_options[OPT_PORT], value, VAL_SIZE);
          break;
     default:
          return TFTP_ERROR;
          break;
     }
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
     /* default options  */
#ifdef HAVE_MTFTP
     handler->data.mtftp_client_port = 76;
     Strncpy(handler->data.mtftp_mcast_ip, "0.0.0.0", MAXLEN);
     handler->data.mtftp_listen_delay = 2;
     handler->data.mtftp_timeout_delay = 2;
#endif
     handler->data.timeout = TIMEOUT;
     handler->data.checkport = 1;
     handler->data.trace = 1;
     handler->data.verbose = 0;
#ifdef DEBUG
     handler->data.delay = 0;
#endif

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
     if (handler->tftp_options[OPT_BLKSIZE][0] != '\0')
     {
          char *set_blksize_argv[] = {
              "option",
              "blksize",
              handler->tftp_options[OPT_BLKSIZE]};
          config_ret |= set_option(3, set_blksize_argv, handler);
     }

     if (handler->tftp_options[OPT_PORT][0] != '\0')
     {
          char *set_port_argv[] = {
              "option",
              "port",
              handler->tftp_options[OPT_PORT]};
          config_ret |= set_option(3, set_port_argv, handler);
     }

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
          if (handler->data.tftp_options[OPT_TSIZE].specified)
               fprintf(stderr, "  tsize:     enabled\n");
          else
               fprintf(stderr, "  tsize:     disabled\n");
          if (handler->data.tftp_options[OPT_BLKSIZE].specified)
               fprintf(stderr, "  blksize:   %s\n",
                       handler->data.tftp_options[OPT_BLKSIZE].value);
          else
               fprintf(stderr, "  blksize:   disabled\n");
          if (handler->data.tftp_options[OPT_TIMEOUT].specified)
               fprintf(stderr, "  timeout:   %s\n",
                       handler->data.tftp_options[OPT_TIMEOUT].value);
          else
               fprintf(stderr, "  timeout:   disabled\n");
          if (handler->data.tftp_options[OPT_MULTICAST].specified)
               fprintf(stderr, "  multicast: enabled\n");
          else
               fprintf(stderr, "  multicast: disabled\n");
          if (handler->data.tftp_options[OPT_PASSWORD].specified)
               fprintf(stderr, "   password: enabled\n");
          else
               fprintf(stderr, "   password: disabled\n");
          if (handler->data.tftp_options[OPT_PORT].specified)
               fprintf(stderr, "   port: enabled\n");
          else
               fprintf(stderr, "   port: disabled\n");
          return ERR;
     }
     /* if disabling an option */
     if (strcasecmp("disable", argv[1]) == 0)
     {
          if (argc != 3)
          {
               fprintf(stderr, "Usage: option disable <option name>\n");
               return ERR;
          }
          /* disable it */
          if (opt_disable_options(handler->data.tftp_options, argv[2]) == ERR)
          {
               fprintf(stderr, "no option named %s\n", argv[2]);
               return ERR;
          }
          if (opt_get_options(handler->data.tftp_options, argv[1], value) == ERR)
               fprintf(stderr, "Option %s disabled\n", argv[2]);
          else
               fprintf(stderr, "Option %s = %s\n", argv[1], value);
          return OK;
     }

     /* ok, we are setting an argument */
     if (argc == 2)
     {
          if (opt_set_options(handler->data.tftp_options, argv[1], NULL) == ERR)
          {
               fprintf(stderr, "no option named %s\n", argv[1]);
               return ERR;
          }
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

#ifdef HAVE_MTFTP
     int use_mtftp;
#endif
     char *tmp;
     socklen_t len = sizeof(handler->data.sa_local);

#ifdef HAVE_MTFTP
     if (strcmp(argv[0], "mget") == 0)
          use_mtftp = 1;
     else
          use_mtftp = 0;
#endif

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
#ifdef HAVE_MTFTP
     if (use_mtftp)
          tftp_result = tftp_mtftp_receive_file(&handler->data);
     else
#endif
          tftp_result = tftp_receive_file(&handler->data);

     gettimeofday(&handler->data.end_time, NULL);

     /* close the socket */
     fsync(handler->data.sockfd);
     close(handler->data.sockfd);

     return tftp_result;
}

#ifdef HAVE_MTFTP
/*
 * Set ot get mtftp variable value
 */
int mtftp_opt(int argc, char **argv, TftpHandlerPtr handler)
{
     if (argc != 3)
     {
          fprintf(stderr, "Usage: mtftp <option name> <option value>\n");
          /* print current value of variables */
          fprintf(stderr, "  client-port:   %d\n", handler->data.mtftp_client_port);
          fprintf(stderr, "  mcast-ip:      %s\n", handler->data.mtftp_mcast_ip);
          fprintf(stderr, "  listen-delay:  %d\n", handler->data.mtftp_listen_delay);
          fprintf(stderr, "  timeout-delay: %d\n", handler->data.mtftp_timeout_delay);
     }
     else
     {
          if (strcmp(argv[1], "client-port") == 0)
          {
               handler->data.mtftp_client_port = atoi(argv[2]);
               fprintf(stderr, "mtftp client-port = %d\n",
                       handler->data.mtftp_client_port);
          }
          else if (strcmp(argv[1], "mcast-ip") == 0)
          {
               Strncpy(handler->data.mtftp_mcast_ip, argv[2], MAXLEN);
               fprintf(stderr, "mtftp mcast-ip = %s\n", handler->data.mtftp_mcast_ip);
          }
          else if (strcmp(argv[1], "listen-delay") == 0)
          {
               handler->data.mtftp_listen_delay = atoi(argv[2]);
               fprintf(stderr, "mtftp listen-delay = %d\n",
                       handler->data.mtftp_listen_delay);
          }
          else if (strcmp(argv[1], "timeout-delay") == 0)
          {
               handler->data.mtftp_timeout_delay = atoi(argv[2]);
               fprintf(stderr, "mtftp timeout-delay = %d\n",
                       handler->data.mtftp_timeout_delay);
          }
          else
          {
               fprintf(stderr, "no mtftp variable named %s\n", argv[1]);
               return ERR;
          }
     }
     return OK;
}

#endif

int set_verbose(int argc, char **argv, TftpHandlerPtr handler)
{
     if (handler->data.verbose)
     {
          handler->data.verbose = 0;
          fprintf(stderr, "Verbose mode off.\n");
     }
     else
     {
          handler->data.verbose = 1;
          fprintf(stderr, "Verbose mode on.\n");
     }
     return OK;
}

int set_trace(int argc, char **argv, TftpHandlerPtr handler)
{
     if (handler->data.trace)
     {
          handler->data.trace = 0;
          fprintf(stderr, "Trace mode off.\n");
     }
     else
     {
          handler->data.trace = 1;
          fprintf(stderr, "Trace mode on.\n");
     }
     return OK;
}

int status(int argc, char **argv, TftpHandlerPtr handler)
{
     struct timeval tmp;

     char string[MAXLEN];

     timeval_diff(&tmp, &handler->data.end_time, &handler->data.start_time);

     if (!handler->data.connected)
          fprintf(stderr, "Not connected\n");
     else
          fprintf(stderr, "Connected:  %s port %d\n", handler->data.hostname,
                  handler->data.port);
     fprintf(stderr, "Mode:       %s\n", handler->data.tftp_options[OPT_MODE].value);
     if (handler->data.verbose)
          fprintf(stderr, "Verbose:    on\n");
     else
          fprintf(stderr, "Verbose:    off\n");
     if (handler->data.trace)
          fprintf(stderr, "Trace:      on\n");
     else
          fprintf(stderr, "Trace:      off\n");
     fprintf(stderr, "Options\n");
     if (handler->data.tftp_options[OPT_TSIZE].specified)
          fprintf(stderr, " tsize:     enabled\n");
     else
          fprintf(stderr, " tsize:     disabled\n");
     if (handler->data.tftp_options[OPT_BLKSIZE].specified)
          fprintf(stderr, " blksize:   enabled\n");
     else
          fprintf(stderr, " blksize:   disabled\n");
     if (handler->data.tftp_options[OPT_TIMEOUT].specified)
          fprintf(stderr, " timeout:   enabled\n");
     else
          fprintf(stderr, " timeout:   disabled\n");
     if (handler->data.tftp_options[OPT_MULTICAST].specified)
          fprintf(stderr, " multicast: enabled\n");
     else
          fprintf(stderr, " multicast: disabled\n");
#ifdef HAVE_MTFTP
     fprintf(stderr, "mtftp variables\n");
     fprintf(stderr, " client-port:   %d\n", handler->data.mtftp_client_port);
     fprintf(stderr, " mcast-ip:      %s\n", handler->data.mtftp_mcast_ip);
     fprintf(stderr, " listen-delay:  %d\n", handler->data.mtftp_listen_delay);
     fprintf(stderr, " timeout-delay: %d\n", handler->data.mtftp_timeout_delay);
#endif

     if (strlen(handler->data.tftp_options[OPT_FILENAME].value) > 0)
     {
          fprintf(stderr, "Last file: %s\n",
                  handler->data.tftp_options[OPT_FILENAME].value);
          if (tftp_result == OK)
          {
               print_eng((double)handler->data.file_size, string, sizeof(string), "%3.3f%cB");
               fprintf(stderr, "  Bytes transferred:  %s\n", string);
               fprintf(stderr, "  Time of transfer: %8.3fs\n",
                       (double)(tmp.tv_sec + tmp.tv_usec * 1e-6));
               fprintf(stderr, "  Throughput:        ");
               print_eng((handler->data.file_size /
                          (double)(tmp.tv_sec + tmp.tv_usec * 1e-6)),
                         string, sizeof(string), "%3.3f%cB/s\n");
               fprintf(stderr, "%s", string);
          }
          else
               fprintf(stderr, "  Transfer aborted\n");
     }
     return OK;
}

int set_timeout(int argc, char **argv, TftpHandlerPtr handler)
{
     if (argc == 1)
          fprintf(stderr, "Timeout set to %d seconds.\n", handler->data.timeout);
     if (argc == 2)
          handler->data.timeout = atoi(argv[1]);
     if (argc > 2)
     {
          fprintf(stderr, "Usage: timeout [value]\n");
          return ERR;
     }
     return OK;
}

#if 0
#define PUT 1
#define GET 2
#define MGET 3

/*
 * If tftp is called with --help, usage is printed and we exit.
 * With --version, version is printed and we exit too.
 * if --get --put --remote-file or --local-file is set, it imply non
 * interactive invocation of tftp.
 */
int tftp_cmd_line_options(int argc, char **argv)
{
     int c;
     int ac;                    /* to format arguments for process_cmd */
     char **av = NULL;          /* same */
     char string[MAXLEN];
     char local_file[MAXLEN] = "";
     char remote_file[MAXLEN] = "";
     int action = 0;

     int option_index = 0;
     static struct option options[] = {
          { "get", 0, NULL, 'g'},
#ifdef HAVE_MTFTP
          { "mget", 0, NULL, 'G'},
#endif
          { "put", 0, NULL, 'p'},
          { "local-file", 1, NULL, 'l'},
          { "remote-file", 1, NULL, 'r'},
          { "password", 1, NULL, 'P'},
          { "tftp-timeout", 1, NULL, 'T'},
          { "mode", 1, NULL, 'M'},
          { "option", 1, NULL, 'O'},
#if 1
          { "timeout", 1, NULL, 't'},
          { "blksize", 1, NULL, 'b'},
          { "tsize", 0, NULL, 's'},
          { "multicast", 0, NULL, 'm'},
#endif
          { "mtftp", 1, NULL, '1'},
          { "no-source-port-checking", 0, NULL, '0'},
          { "prevent-sas", 0, NULL, 'X'},
          { "verbose", 0, NULL, 'v'},
          { "trace", 0, NULL, 'd'},
#if DEBUG
          { "delay", 1, NULL, 'D'},
#endif
          { "version", 0, NULL, 'V'},
          { "help", 0, NULL, 'h' },
          { 0, 0, 0, 0 }
     };

     /* Support old argument until 0.8 */
     while ((c = getopt_long(argc, argv, /*"gpl:r:Vh"*/ "gpl:r:Vht:b:smP:",
                             options, &option_index)) != EOF)
     {
          switch (c)
          {
          case 'g':
               interactive = 0;
               if ((action == PUT) || (action == MGET))
               {
                    fprintf(stderr, "two actions specified!\n");
                    exit(ERR);
               }
               else
                    action = GET;
               break;
          case 'G':
               interactive = 0;
               if ((action == PUT) || (action == GET))
               {
                    fprintf(stderr, "two actions specified!\n");
                    exit(ERR);
               }
               else
                    action = MGET;
               break;
          case 'p':
               interactive = 0;
               if ((action == GET) || (action == MGET))
               {
                    fprintf(stderr, "two actions specified!\n");
                    exit(ERR);
               }
               else
                    action = PUT;
               break;
          case 'P':
               snprintf(string, sizeof(string), "option password %s", optarg);
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case 'l':
               interactive = 0;
               Strncpy(local_file, optarg, MAXLEN);
               break;
          case 'r':
               interactive = 0;
               Strncpy(remote_file, optarg, MAXLEN);
               break;
          case 'T':
               snprintf(string, sizeof(string), "timeout %s", optarg);
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case 'M':
               snprintf(string, sizeof(string), "mode %s", optarg);
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case 'O':
               snprintf(string, sizeof(string), "option %s", optarg);
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case '1':
               snprintf(string, sizeof(string), "mtftp %s", optarg);
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
#if 1
          case 't':
               fprintf(stderr, "--timeout deprecated, use --option instead\n");
               snprintf(string, sizeof(string), "option timeout %s", optarg);
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case 'b':
               fprintf(stderr, "--blksize deprecated, use --option instead\n");
               snprintf(string, sizeof(string), "option blksize %s", optarg);
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case 's':
               fprintf(stderr, "--tsize deprecated, use --option instead\n");
               snprintf(string, sizeof(string), "option tsize");
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case 'm':
               fprintf(stderr,
                       "--multicast deprecated, use --option instead\n");
               snprintf(string, sizeof(string), "option multicast");
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
#endif
          case '0':
               handler->data.checkport = 0;
               break;
          case 'X':
               tftp_prevent_sas = 1;
               break;
          case 'v':
               snprintf(string, sizeof(string), "verbose on");
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
          case 'd':
               snprintf(string, sizeof(string), "trace on");
               make_arg(string, &ac, &av);
               process_cmd(ac, av);
               break;
#if DEBUG
          case 'D':
               handler->data.delay = atoi(optarg);
               break;
#endif
          case 'V':
               fprintf(stderr, "atftp-%s (client)\n", VERSION);
               exit(OK);
          case 'h':
               tftp_usage();
               exit(OK);
          case '?':
               tftp_usage();
               exit(ERR);
               break;
          }
     }

     /* verify that one or two arguements are left, they are the host name
        and port */
     /* optind point to the first non option caracter */
     if (optind < (argc - 2))
     {
          tftp_usage();
          exit(ERR);
     }

     if (optind != argc)
     {
          if (optind == (argc - 1))
               snprintf(string, sizeof(string), "connect %s", argv[optind]);
          else
               snprintf(string, sizeof(string), "connect %s %s", argv[optind],
                                       argv[optind+1]);
          make_arg(string, &ac, &av);
          process_cmd(ac, av);
     }

     if (!interactive)
     {
          if (action == PUT)
          {
               if(strlen(remote_file) == 0)
               {
                   strncpy(remote_file, local_file, MAXLEN);
               }
               make_arg_vector(&ac,&av,"put",local_file,remote_file,NULL);
          }
          else if (action == GET)
          {
               if(strlen(local_file) == 0)
               {
                   strncpy(local_file, remote_file, MAXLEN);
               }
               make_arg_vector(&ac,&av,"get",remote_file,local_file,NULL);
          }
          else if (action == MGET)
          {
               if(strlen(local_file) == 0)
               {
                   strncpy(local_file, remote_file, MAXLEN);
               }
               make_arg_vector(&ac,&av,"mget",remote_file,local_file,NULL);
          }
          else
          {
               fprintf(stderr, "No action specified in batch mode!\n");
               exit(ERR);
          }
          if (process_cmd(ac, av) == ERR)
               exit(ERR);
     }
     return OK;
}
#endif
