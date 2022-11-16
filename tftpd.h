// Modified by Kollins G. Lima <kgl2@cin.ufpe.br>
/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftpd.h
 *
 * $Id: tftpd.h,v 1.22 2004/02/27 02:05:26 jp Exp $
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

#ifndef tftpd_h
#define tftpd_h

#include <pthread.h>
#include <arpa/tftp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include "tftp_io.h"
#include "tftpd_api.h"

/*
 * Per thread data. There is a thread for each client or group
 * (multicast) of client.
 */
struct thread_data {
     pthread_t tid;

     /* private to thread */
     char *data_buffer;
     int data_buffer_size;

     int timeout;
     int checkport;             /* Disable TID check. Violate RFC */
     int mcast_switch_client;   /* When set, server will switch to next client
                                   on first timeout from the current client. */
     int trace;

     TftpdHandlerPtr handler;
     TftpdSectionHandlerPtr section_handler_ptr;

     int sockfd;

     /* callback for opening file */
     void *open_file_ctx;
     open_file_callback open_file_cb;
     /* callback for closing file */
     void *close_file_ctx;
     close_file_callback close_file_cb;
     /* callback for section started */
     void *section_started_ctx;
     section_started section_started_cb;
     /* callback for section finished */
     void *section_finished_ctx;
     section_finished section_finished_cb;
     /* callback for option received */
     void *option_received_ctx;
     option_received_callback option_received_cb;

    int *tftpd_cancel;
    pthread_mutex_t *stdin_mutex;

     /* multicast stuff */
     short mc_port;             /* multicast port */
     char *mc_addr;             /* multicast address */
     struct sockaddr_storage sa_mcast;
     union ip_mreq_storage mcastaddr;
     u_char mcast_ttl;
     
     /*
      * Self can read/write until client_ready is set. Then only allowed to read.
      * Other thread can read it only when client_ready is set. Remember that access
      * to client_ready bellow is protected by a mutex.
      */
     struct tftp_opt *tftp_options;
 
     /* 
      * Must lock to insert in the list or search, but not to read or write 
      * in the client_info structure, since only the propriotary thread do it.
      */
     pthread_mutex_t client_mutex;
     struct client_info *client_info;
     int client_ready;        /* one if other thread may add client */
 
     /* must be lock (list lock) to update */
     struct thread_data *prev;
     struct thread_data *next;
};

struct client_info {
     struct sockaddr_storage client;
     int done;                  /* that client as receive it's file */
     struct client_info *next;
};


struct TftpdHandler
{
     void *open_file_context;
     open_file_callback open_file_cb;
     void *close_file_context;
     close_file_callback close_file_cb;
     void *section_started_context;
     section_started section_started_cb;
     void *section_finished_context;
     section_finished section_finished_cb;
     option_received_callback option_received_cb;
     void *option_received_context;
     int tftpd_port;    /* Port atftpd listen to */
     int tftpd_timeout; /* number of second of inactivity
                             before exiting */
     int tftpd_cancel;  /* When true, thread must exit. pthread
                           cancellation point are not used because
                           thread id are not tracked. */
     pthread_t main_thread_id;
     int stop_pipe[2];

     /*
      * "logging level" is the maximum error level that will get logged.
      *  This can be increased with the -v switch.
      */
     int logging_level;
     char *log_file;

     /*
      * We need a lock on stdin from the time we notice fresh data coming from
      * stdin to the time the freshly created server thread as read it.
      */
     pthread_mutex_t stdin_mutex;

     struct thread_data *thread_data; /* head of thread list */
     int number_of_thread;

     pthread_mutex_t thread_list_mutex;
};

struct TftpdSectionHandler
{
     SectionId section_id;
     char client_ip[SOCKADDR_PRINT_ADDR_LEN];
     TftpdSectionStatus status;
     char *error_msg;
     int abort; /* 1 if we need to abort because the maximum
                  number of threads have been reached*/
};

/*
 * Functions defined in tftpd_file.c
 */
int tftpd_rules_check(char *filename);
int tftpd_receive_file(struct thread_data *data);
int tftpd_send_file(struct thread_data *data);

/*
 * Defined in tftpd_list.c, operation on thread_data list.
 */
int tftpd_list_add(TftpdHandlerPtr handler, struct thread_data *new);
int tftpd_list_remove(TftpdHandlerPtr handler, struct thread_data *old);
int tftpd_list_num_of_thread(TftpdHandlerPtr handler);
int tftpd_list_find_multicast_server_and_add(TftpdHandlerPtr handler,
                                             struct thread_data **thread,
                                             struct thread_data *data,
                                             struct client_info *client);
/*
 * Defined in tftpd_list.c, operation on client structure list.
 */
void tftpd_clientlist_ready(TftpdHandlerPtr handler, struct thread_data *thread);
void tftpd_clientlist_remove(TftpdHandlerPtr handler,
                             struct thread_data *thread,
                             struct client_info *client);
void tftpd_clientlist_free(TftpdHandlerPtr handler, struct thread_data *thread);
int tftpd_clientlist_done(TftpdHandlerPtr handler,
                          struct thread_data *thread,
                          struct client_info *client,
                          struct sockaddr_storage *sock);
int tftpd_clientlist_next(TftpdHandlerPtr handler,
                          struct thread_data *thread,
                          struct client_info **client);
void tftpd_list_kill_threads(TftpdHandlerPtr handler);

/*
 * Defines in tftpd_mcast.c
 */
int tftpd_mcast_get_tid(char **addr, short *port);
int tftpd_mcast_free_tid(char *addr, short port);
int tftpd_mcast_parse_opt(char *addr, char *ports);

#endif
