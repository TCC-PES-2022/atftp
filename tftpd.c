// Modified by Kollins G. Lima <kgl2@cin.ufpe.br>
/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftpd.c
 *    main server file
 *
 * $Id: tftpd.c,v 1.61 2004/02/27 02:05:26 jp Exp $
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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>

#include "tftpd.h"
#include "tftp_io.h"
#include "tftp_def.h"
#include "atftp_logger.h"
#include "options.h"

#include <stdlib.h>
#include "tftpd_api.h"
#define MAX_PARAM_SIZE 32

/*
 * Global variables set by main when starting. Read-only for threads
 */
int tftpd_max_thread = 100; /* number of concurent thread allowed */

// char directory[MAXLEN] = "/srv/tftp/";
char directory[MAXLEN] = "./";
int retry_timeout = S_TIMEOUT;

int on = 1;

int drop_privs = 0;           /* whether it was explicitly requested to switch to another user. */
char tftpd_addr[MAXLEN] = ""; /* IP address atftpd binds to */

/*
 * Multicast related. These reflect option from command line
 * argument --mcast-ttl, --mcast-port and  --mcast-addr
 */
int mcast_ttl = 1;
char mcast_addr[MAXLEN] = "239.255.0.0-255";
char mcast_port[MAXLEN] = "1758";

/* logging level as requested by libwrap */
#ifdef HAVE_WRAP
int allow_severity = LOG_NOTICE;
int deny_severity = LOG_NOTICE;
#endif

/* user ID and group ID when running as a daemon */
char user_name[MAXLEN] = "nobody";
char group_name[MAXLEN] = "nogroup";

/* For special uses, disable source port checking */
int source_port_checking = 1;

/* Special feature to make the server switch to new client as soon as
   the current client timeout in multicast mode */
int mcast_switch_client = 0;

int trace = 1;

/*
 * Function defined in this file
 */
void *tftpd_receive_request(void *);
void tftpd_log_options(struct TftpdHandler *handler);
void tftpd_usage(void);

TftpdOperationResult create_tftpd_handler(TftpdHandlerPtr *handler)
{
     (*handler) = (TftpdHandlerPtr)malloc(sizeof(struct TftpdHandler));

     (*handler)->tftpd_port = 69;
     (*handler)->tftpd_timeout = 300;
     (*handler)->open_file_cb = NULL;
     (*handler)->close_file_cb = NULL;
     (*handler)->section_started_cb = NULL;
     (*handler)->section_finished_cb = NULL;
     (*handler)->open_file_context = NULL;
     (*handler)->close_file_context = NULL;
     (*handler)->section_started_context = NULL;
     (*handler)->section_finished_context = NULL;
     (*handler)->tftpd_cancel = 0;
     (*handler)->logging_level = LOG_DEBUG;
     (*handler)->log_file = NULL;
     pthread_mutex_init(&(*handler)->stdin_mutex, NULL);
     pipe((*handler)->stop_pipe);
     fcntl((*handler)->stop_pipe[1], F_SETFL, O_NONBLOCK);

     (*handler)->thread_data = NULL;
     (*handler)->number_of_thread = 0;
     pthread_mutex_init(&(*handler)->thread_list_mutex, NULL);

     return TFTPD_OK;
}

TftpdOperationResult destroy_tftpd_handler(TftpdHandlerPtr *handler)
{
     if ((*handler) == NULL)
     {
          return TFTPD_ERROR;
     }
     (*handler)->open_file_cb = NULL;
     (*handler)->close_file_cb = NULL;
     (*handler)->section_started_cb = NULL;
     (*handler)->section_finished_cb = NULL;
     (*handler)->close_file_context = NULL;
     (*handler)->section_started_context = NULL;
     (*handler)->section_finished_context = NULL;
     if ((*handler)->log_file != NULL)
     {
          free((*handler)->log_file);
          (*handler)->log_file = NULL;
     }
     close((*handler)->stop_pipe[0]);
     close((*handler)->stop_pipe[1]);
     free((*handler));
     (*handler) = NULL;
     return TFTPD_OK;
}

TftpdOperationResult set_port(
    const TftpdHandlerPtr handler,
    const int port)
{
     if (handler == NULL)
     {
          return TFTPD_ERROR;
     }
     if (port < 0)
     {
          return TFTPD_ERROR;
     }
     handler->tftpd_port = port;
     return TFTPD_OK;
}

TftpdOperationResult set_server_timeout(
    const TftpdHandlerPtr handler,
    const int timeout)
{
     if (handler == NULL)
     {
          return TFTPD_ERROR;
     }
     if (timeout < 0)
     {
          return TFTPD_ERROR;
     }
     handler->tftpd_timeout = timeout;
     return TFTPD_OK;
}

TftpdOperationResult register_open_file_callback(
    const TftpdHandlerPtr handler,
    open_file_callback callback,
    void *context)
{
     if (handler == NULL)
     {
          return TFTPD_ERROR;
     }
     handler->open_file_cb = callback;
     handler->open_file_context = context;
     return TFTPD_OK;
}

TftpdOperationResult register_close_file_callback(
    const TftpdHandlerPtr handler,
    close_file_callback callback,
    void *context)
{
     if (handler == NULL)
     {
          return TFTPD_ERROR;
     }
     handler->close_file_cb = callback;
     handler->close_file_context = context;
     return TFTPD_OK;
}

TftpdOperationResult register_section_started_callback(
    const TftpdHandlerPtr handler,
    section_started callback,
    void *context)
{
     if (handler == NULL)
     {
          return TFTPD_ERROR;
     }
     handler->section_started_cb = callback;
     handler->section_started_context = context;
     return TFTPD_OK;
}

TftpdOperationResult register_section_finished_callback(
    const TftpdHandlerPtr handler,
    section_finished callback,
    void *context)
{
     if (handler == NULL)
     {
          return TFTPD_ERROR;
     }
     handler->section_finished_cb = callback;
     handler->section_finished_context = context;
     return TFTPD_OK;
}

TftpdOperationResult get_section_id(
    const TftpdSectionHandlerPtr section_handler,
    SectionId *id)
{
     if (section_handler == NULL)
     {
          return TFTPD_ERROR;
     }
     *id = section_handler->section_id;
     return TFTPD_OK;
}

TftpdOperationResult get_client_ip(
    const TftpdSectionHandlerPtr section_handler,
    char *ip)
{
     if (section_handler == NULL)
     {
          return TFTPD_ERROR;
     }
     memcpy(ip, section_handler->client_ip, sizeof(section_handler->client_ip));
     return TFTPD_OK;
}

TftpdOperationResult get_section_status(
    const TftpdSectionHandlerPtr section_handler,
    TftpdSectionStatus *status)
{
     if (section_handler == NULL)
     {
          return TFTPD_ERROR;
     }
     *status = section_handler->status;
     return TFTPD_OK;
}

TftpdOperationResult set_error_msg(
    const TftpdSectionHandlerPtr section_handler,
    const char *error_msg)
{
     if (section_handler == NULL)
     {
          return TFTPD_ERROR;
     }
     if (error_msg == NULL)
     {
          return TFTPD_ERROR;
     }
     section_handler->error_msg = strdup(error_msg);
     return TFTPD_OK;
}

TftpdOperationResult get_error_msg(
    const TftpdSectionHandlerPtr section_handler,
    char **error_msg)
{
     if (section_handler == NULL)
     {
          return TFTPD_ERROR;
     }
     if (section_handler->error_msg == NULL)
     {
          return TFTPD_ERROR;
     }

     (*error_msg) = strdup(section_handler->error_msg);
     return TFTPD_OK;
}

TftpdOperationResult stop_listening(
    const TftpdHandlerPtr handler)
{
     if (handler == NULL)
     {
          return TFTPD_ERROR;
     }
     handler->tftpd_cancel = 1;

     // Send signal to thread to force return from select.
     write(handler->stop_pipe[1], "\0", 1);
     return TFTPD_OK;
}

/*
 * Main thread. Do required initialisation and then go through a loop
 * listening for client requests. When a request arrives, we allocate
 * memory for a thread data structure and start a thread to serve the
 * new client. If theres no activity for more than 'tftpd_timeout'
 * seconds, we exit and tftpd must be respawned by inetd.
 */
TftpdOperationResult start_listening(const TftpdHandlerPtr handler)
{
     fd_set rfds;                /* for select */
     struct timeval tv;          /* for select */
     int run = 1;                /* while (run) loop */
     struct thread_data *new;    /* for allocation of new thread_data */
     int sockfd;                 /* used in daemon mode */
     struct sockaddr_storage sa; /* used in daemon mode */
     struct passwd *user;
     struct group *group;
     pthread_t tid;

     // Set default options
     tftp_default_options[OPT_MULTICAST].enabled = 0;
     int one = 1; /* for setsockopt() */

     /* Using syslog facilties through a wrapper. This call divert logs
      * to a file as specified or to syslog if no file specified. Specifying
      * /dev/stderr or /dev/stdout will work if the server is started in
      * daemon mode.
      */
     handler->log_file = strdup("-"); /* default to stdout */
     open_atftp_logger("atftpd", handler->log_file, handler->logging_level);
     atftp_logger(LOG_NOTICE, "Advanced Trivial FTP server started (%s)", VERSION);

     /* find the port; initialise sockaddr_storage structure */
     if (strlen(tftpd_addr) > 0 || handler->tftpd_port == 0)
     {
          struct addrinfo hints, *result;
          int err;

          /* look up the service and host */
          memset(&hints, 0, sizeof(hints));
          hints.ai_socktype = SOCK_DGRAM;
          hints.ai_flags = AI_NUMERICHOST;
          err = getaddrinfo(strlen(tftpd_addr) ? tftpd_addr : NULL, handler->tftpd_port ? NULL : "tftp",
                            &hints, &result);
          if (err == EAI_SERVICE)
          {
               atftp_logger(LOG_ERR, "atftpd: udp/tftp, unknown service");
               return TFTPD_ERROR;
          }
          if (err || sockaddr_set_addrinfo(&sa, result))
          {
               atftp_logger(LOG_ERR, "atftpd: invalid IP address %s", tftpd_addr);
               return TFTPD_ERROR;
          }

          if (!handler->tftpd_port)
               handler->tftpd_port = sockaddr_get_port(&sa);
          else
          {
               sa.ss_family = AF_INET;
               sockaddr_set_port(&sa, handler->tftpd_port);
          }

          freeaddrinfo(result);
     }

     if (strlen(tftpd_addr) == 0)
     {
          memset(&sa, 0, sizeof(sa));
          sa.ss_family = AF_INET;
     }

     sockaddr_set_port(&sa, handler->tftpd_port);

     /* open the socket */
     if ((sockfd = socket(sa.ss_family, SOCK_DGRAM, 0)) == 0)
     {
          atftp_logger(LOG_ERR, "atftpd: can't open socket");
          return TFTPD_ERROR;
     }
     /* bind the socket to the desired address and port  */
     if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
     {
          atftp_logger(LOG_ERR, "atftpd: can't bind port %s:%d/udp",
                       tftpd_addr, handler->tftpd_port);
          return TFTPD_ERROR;
     }

     /* release priviliedge */

     /* first see if we are or can somehow become root, if so prepare
      * for drop even if not requested on command line */
     if (geteuid() == 0)
     {
          drop_privs = 1;
     }
     else if (getuid() == 0)
     {
          if (seteuid(0) == 0)
               drop_privs = 1;
     }

     if (drop_privs)
     {
          user = getpwnam(user_name);
          group = getgrnam(group_name);
          if (!user || !group)
          {
               atftp_logger(LOG_ERR,
                            "atftpd: can't change identity to %s.%s: no such user/group. exiting.",
                            user_name, group_name);
               return TFTPD_ERROR;
          }
     }
     else
     { /* make null pointers to prevent goofing up in case we accidenally access them */
          user = NULL;
          group = NULL;
     }

     if (drop_privs)
     {
          if (setregid(group->gr_gid, group->gr_gid) == -1)
          {
               atftp_logger(LOG_ERR, "atftpd: failed to setregid to %s.", group_name);
               return TFTPD_ERROR;
          }
          if (setreuid(user->pw_uid, user->pw_uid) == -1)
          {
               atftp_logger(LOG_ERR, "atftpd: failed to setreuid to %s.", user_name);
               return TFTPD_ERROR;
          }
     }

     /* Reopen log file now that we changed user */
     open_atftp_logger("atftpd", handler->log_file, handler->logging_level);

#if defined(SOL_IP) && defined(IP_PKTINFO)
     /* We need to retieve some information from incoming packets */
     if (setsockopt(sockfd, SOL_IP, IP_PKTINFO, &one, sizeof(one)) != 0)
     {
          atftp_logger(LOG_WARNING, "Failed to set socket option: %s", strerror(errno));
     }
#endif

     /* save main thread ID for proper signal handling */
     handler->main_thread_id = pthread_self();

     /* print summary of options */
     tftpd_log_options(handler);

     /* Wait for read or write request and exit if timeout. */
     while (run)
     {
          FD_ZERO(&rfds);
          FD_SET(sockfd, &rfds);
          FD_SET(handler->stop_pipe[0], &rfds);
          tv.tv_sec = handler->tftpd_timeout;
          tv.tv_usec = 0;

          /* We need to lock stdin, and release it when the thread
             is done reading the request. */
          pthread_mutex_lock(&handler->stdin_mutex);

          /* A timeout of 0 is interpreted as infinity. Wait for incoming
             packets */
          if (!handler->tftpd_cancel)
          {
               int rv;

               //               if ((tftpd_timeout == 0) || (tftpd_daemon))
               if ((handler->tftpd_timeout == 0))
                    rv = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
               else
                    rv = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);
               if (rv < 0)
               {
                    atftp_logger(LOG_ERR, "%s: %d: select: %s",
                                 __FILE__, __LINE__, strerror(errno));
                    /* Clear the bits, they are undefined! */
                    FD_ZERO(&rfds);
               }
          }

          if (FD_ISSET(sockfd, &rfds) && (!handler->tftpd_cancel))
          {
               /* Allocate memory for thread_data structure. */
               if ((new = calloc(1, sizeof(struct thread_data))) == NULL)
               {
                    atftp_logger(LOG_ERR, "%s: %d: Memory allocation failed",
                                 __FILE__, __LINE__);
                    return TFTPD_ERROR;
               }

               /*
                * Initialisation of thread_data structure.
                */
               pthread_mutex_init(&new->client_mutex, NULL);
               new->sockfd = sockfd;
               new->handler = handler;
               new->open_file_ctx = handler->open_file_context;
               new->open_file_cb = handler->open_file_cb;
               new->close_file_ctx = handler->close_file_context;
               new->close_file_cb = handler->close_file_cb;
               new->section_started_ctx = handler->section_started_context;
               new->section_started_cb = handler->section_started_cb;
               new->section_finished_ctx = handler->section_finished_context;
               new->section_finished_cb = handler->section_finished_cb;
               new->tftpd_cancel = &(handler->tftpd_cancel);
               new->stdin_mutex = &(handler->stdin_mutex);

               /* Allocate data buffer for tftp transfer. */
               if ((new->data_buffer = malloc((size_t)SEGSIZE + 4)) == NULL)
               {
                    atftp_logger(LOG_ERR, "%s: %d: Memory allocation failed",
                                 __FILE__, __LINE__);
                    return TFTPD_ERROR;
               }
               new->data_buffer_size = SEGSIZE + 4;

               /* Allocate memory for tftp option structure. */
               if ((new->tftp_options =
                        malloc(sizeof(tftp_default_options))) == NULL)
               {
                    atftp_logger(LOG_ERR, "%s: %d: Memory allocation failed",
                                 __FILE__, __LINE__);
                    return TFTPD_ERROR;
               }

               /* Copy default options. */
               memcpy(new->tftp_options, tftp_default_options,
                      sizeof(tftp_default_options));

               /* default timeout */
               new->timeout = retry_timeout;

               /* wheter we check source port or not */
               new->checkport = source_port_checking;

               /* other options */
               new->mcast_switch_client = mcast_switch_client;
               new->trace = trace;

               /* default ttl for multicast */
               new->mcast_ttl = mcast_ttl;

               /* Allocate memory for a client structure. */
               if ((new->client_info =
                        calloc(1, sizeof(struct client_info))) == NULL)
               {
                    atftp_logger(LOG_ERR, "%s: %d: Memory allocation failed",
                                 __FILE__, __LINE__);
                    return TFTPD_ERROR;
               }
               new->client_info->done = 0;
               new->client_info->next = NULL;

               /* Start a new server thread. */
               if (pthread_create(&tid, NULL, tftpd_receive_request,
                                  (void *)new) != 0)
               {
                    atftp_logger(LOG_ERR, "Failed to start new thread");
                    free(new->data_buffer);
                    free(new->tftp_options);
                    free(new->client_info);
                    free(new);
                    pthread_mutex_unlock(&handler->stdin_mutex);
               }
          }
          else
          {
               pthread_mutex_unlock(&handler->stdin_mutex);

               /* Either select return after timeout of we've been killed. In the first case
                  we wait for server thread to finish, in the other we kill them */
               if (handler->tftpd_cancel)
                    tftpd_list_kill_threads(handler);

               /*
                * TODO: Some times we get stuck here. For now, adding max wait time
                * to avoid this.
                */
               int max_wait = 3;
               while ((tftpd_list_num_of_thread(handler) != 0) && (max_wait > 0))
               {
                    sleep(1);
                    max_wait--;
               }

               run = 0;
               atftp_logger(LOG_NOTICE, "atftpd terminating");
          }
     }

     /* close all open file descriptors */
     close(sockfd);

     atftp_logger(LOG_NOTICE, "Main thread exiting");
     close_atftp_logger();
     return TFTPD_OK;
}

static void tftpd_receive_request_cleanup(void *arg)
{
     struct thread_data *data = (struct thread_data *)arg;

     /* Remove the thread_data structure from the list, if it as been
        added. */
     if (!data->section_handler_ptr->abort)
          tftpd_list_remove(data->handler, data);

     /* Free memory. */
     if (data->data_buffer)
          free(data->data_buffer);

     if (data->section_handler_ptr->error_msg)
          free(data->section_handler_ptr->error_msg);

     free(data->tftp_options);

     /* this function take care of freeing allocated memory by other threads */
     tftpd_clientlist_free(data->handler, data);

     if (data->section_finished_cb != NULL)
          data->section_finished_cb(data->section_handler_ptr,
                                    data->section_finished_ctx);

     /* free the thread structure */
     free(data);

     atftp_logger(LOG_INFO, "Server thread exiting");
}

/*
 * This function handles the initial connection with a client. It reads
 * the request from stdin and then release the stdin_mutex, so the main
 * thread may listen for new clients. After that, we process options and
 * call the sending or receiving function.
 *
 * arg is a thread_data structure pointer for that thread.
 */
void *tftpd_receive_request(void *arg)
{
     pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
     pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

     struct thread_data *data = (struct thread_data *)arg;
     pthread_cleanup_push(tftpd_receive_request_cleanup, data);

     int retval;          /* hold return value for testing */
     int data_size;       /* returned size by recvfrom */
     char string[MAXLEN]; /* hold the string we pass to the atftp_logger */
     int num_of_threads;

     struct sockaddr_storage to; /* destination of client's packet */
     socklen_t len = sizeof(to);

     char addr_str[SOCKADDR_PRINT_ADDR_LEN];

     /* Detach ourself. That way the main thread does not have to
      * wait for us with pthread_join. */
     data->tid = pthread_self();
     pthread_detach(data->tid);

     /* Read the first packet from stdin. */
     data_size = data->data_buffer_size;
     retval = tftp_get_packet(data->sockfd, -1, NULL, &data->client_info->client, NULL,
                              &to, data->timeout, &data_size,
                              data->data_buffer);

     struct TftpdSectionHandler section_handler;
     section_handler.section_id = data->tid;
     section_handler.status = TFTPD_SECTION_UNDEFINED;
     section_handler.abort = 0;
     section_handler.error_msg = NULL;
     sockaddr_print_addr(&data->client_info->client,
                         section_handler.client_ip,
                         sizeof(section_handler.client_ip));
     data->section_handler_ptr = &section_handler;

     if (retval == ERR)
     {
          atftp_logger(LOG_NOTICE, "Invalid request in 1st packet");
          section_handler.abort = 1;
     }

     if (data->section_started_cb != NULL)
          data->section_started_cb(&section_handler, data->section_started_ctx);

     /* now unlock stdin */
     pthread_mutex_unlock(data->stdin_mutex);

     /* Verify the number of threads */
     if ((num_of_threads = tftpd_list_num_of_thread(data->handler)) >= tftpd_max_thread)
     {
          atftp_logger(LOG_INFO, "Maximum number of threads reached: %d",
                       num_of_threads);
          section_handler.abort = 1;
     }

     /* Add this new thread structure to the list. */
     if (!section_handler.abort)
     {
          tftpd_list_add(data->handler, data);
     }

     /* if the maximum number of thread is reached, too bad we section_handler.abort. */
     if (!section_handler.abort)
     {
          /* open a socket for client communication */
          data->sockfd = socket(data->client_info->client.ss_family,
                                SOCK_DGRAM, 0);
          /*memset(&to, 0, sizeof(to));*/
          /* PSz 11 Aug 2011  http://bugs.debian.org/613582
           * Do not nullify "to", preserve IP address from tftp_get_packet().
           * Only set port to 0, as we used to in v6.
           * (Why set ss_family, was not it right already??)
           */
          to.ss_family = data->client_info->client.ss_family;
          sockaddr_set_port(&to, 0);

          atftp_logger(LOG_INFO, "socket may listen on any address, including broadcast");

          if (data->sockfd > 0)
          {
               /* bind the socket to the interface */
               if (bind(data->sockfd, (struct sockaddr *)&to, len) == -1)
               {
                    atftp_logger(LOG_ERR, "bind: %s", strerror(errno));
                    retval = ABORT;
               }
               /* read back assigned port */
               len = sizeof(to);
               if (getsockname(data->sockfd, (struct sockaddr *)&to, &len) == -1)
               {
                    atftp_logger(LOG_ERR, "getsockname: %s", strerror(errno));
                    retval = ABORT;
               }
               /* connect the socket, faster for kernel operation */
               /* this is not a good idea on FreeBSD, because sendto() cannot
                  be used on a connected datagram socket */
#if !defined(__FreeBSD_kernel__)
               if (connect(data->sockfd,
                           (struct sockaddr *)&data->client_info->client,
                           sizeof(data->client_info->client)) == -1)
               {
                    atftp_logger(LOG_ERR, "connect: %s", strerror(errno));
                    retval = ABORT;
               }
#endif
               atftp_logger(LOG_DEBUG, "Creating new socket: %s:%d",
                            sockaddr_print_addr(&to, addr_str, sizeof(addr_str)),
                            sockaddr_get_port(&to));

               /* read options from request */
               opt_parse_request(data->data_buffer, data_size,
                                 data->tftp_options);
               opt_request_to_string(data->tftp_options, string, MAXLEN);
          }
          else
          {
               retval = ABORT;
          }

          /* Analyse the request. */
          switch (retval)
          {
          case GET_RRQ:
               atftp_logger(LOG_NOTICE, "Serving %s to %s:%d",
                            data->tftp_options[OPT_FILENAME].value,
                            sockaddr_print_addr(&data->client_info->client,
                                                addr_str, sizeof(addr_str)),
                            sockaddr_get_port(&data->client_info->client));
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "received RRQ <%s>", string);
               if (tftpd_send_file(data) == OK)
               {
                    section_handler.status = TFTPD_SECTION_OK;
               }
               else
               {
                    section_handler.status = TFTPD_SECTION_ERROR;
               }
               break;
          case GET_WRQ:
               atftp_logger(LOG_NOTICE, "Fetching from %s to %s",
                            sockaddr_print_addr(&data->client_info->client,
                                                addr_str, sizeof(addr_str)),
                            data->tftp_options[OPT_FILENAME].value);
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "received WRQ <%s>", string);
               if (tftpd_receive_file(data) == OK)
               {
                    section_handler.status = TFTPD_SECTION_OK;
               }
               else
               {
                    section_handler.status = TFTPD_SECTION_ERROR;
               }
               break;
          case ERR:
               atftp_logger(LOG_ERR, "Error from tftp_get_packet");
               tftp_send_error(data->sockfd, &data->client_info->client,
                               EUNDEF, data->data_buffer, data->data_buffer_size,
                               NULL);
               section_handler.status = TFTPD_SECTION_ERROR;
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EUNDEF,
                                 tftp_errmsg[EUNDEF]);
               break;
          case ABORT:
               section_handler.status = TFTPD_SECTION_ERROR;
               if (data->trace)
                    atftp_logger(LOG_ERR, "thread aborting");
               break;
          default:
               section_handler.status = TFTPD_SECTION_ERROR;
               atftp_logger(LOG_NOTICE, "Invalid request <%d> from %s",
                            retval,
                            sockaddr_print_addr(&data->client_info->client,
                                                addr_str, sizeof(addr_str)));
               tftp_send_error(data->sockfd, &data->client_info->client,
                               EBADOP, data->data_buffer, data->data_buffer_size,
                               NULL);
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EBADOP,
                                 tftp_errmsg[EBADOP]);
          }
     }

     /* make sure all data is sent to the network */
     if (data->sockfd)
     {
          fsync(data->sockfd);
          close(data->sockfd);
     }

     pthread_cleanup_pop(data);
     pthread_exit(NULL);
}

/*
 * Output option to the syslog.
 */
void tftpd_log_options(struct TftpdHandler *handler)
{
     atftp_logger(LOG_INFO, "  running in daemon mode on port %d", handler->tftpd_port);
     if (strlen(tftpd_addr) > 0)
          atftp_logger(LOG_INFO, "  bound to IP address %s only", tftpd_addr);

     atftp_logger(LOG_INFO, "  logging level: %d", handler->logging_level);
     if (trace)
          atftp_logger(LOG_INFO, "     trace enabled");
     atftp_logger(LOG_INFO, "  directory: %s", directory);
     atftp_logger(LOG_INFO, "  user: %s.%s", user_name, group_name);
     atftp_logger(LOG_INFO, "  log file: %s", (handler->log_file == NULL) ? "syslog" : handler->log_file);
     atftp_logger(LOG_INFO, "  forcing to listen on local interfaces: on.");
     atftp_logger(LOG_INFO, "  server timeout: Not used");
     atftp_logger(LOG_INFO, "  tftp retry timeout: %d", retry_timeout);
     atftp_logger(LOG_INFO, "  maximum number of thread: %d", tftpd_max_thread);
     atftp_logger(LOG_INFO, "  option timeout:   %s",
                  tftp_default_options[OPT_TIMEOUT].enabled ? "enabled" : "disabled");
     atftp_logger(LOG_INFO, "  option tzise:     %s",
                  tftp_default_options[OPT_TSIZE].enabled ? "enabled" : "disabled");
     atftp_logger(LOG_INFO, "  option blksize:   %s",
                  tftp_default_options[OPT_BLKSIZE].enabled ? "enabled" : "disabled");
     atftp_logger(LOG_INFO, "  option multicast: %s",
                  tftp_default_options[OPT_MULTICAST].enabled ? "enabled" : "disabled");
     atftp_logger(LOG_INFO, "     address range: %s", mcast_addr);
     atftp_logger(LOG_INFO, "     port range:    %s", mcast_port);

     if (!source_port_checking)
          atftp_logger(LOG_INFO, "  --no-source-port-checking turned on");
}