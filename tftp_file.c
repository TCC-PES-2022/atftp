// Modified by Kollins G. Lima <kgl2@cin.ufpe.br>
/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftp_file.c
 *    client side file operations. File receiving and sending.
 *
 * $Id: tftp_file.c,v 1.42 2004/02/13 03:16:09 jp Exp $
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/tftp.h>
#include <netdb.h>
#include <string.h>
#include <sys/stat.h>
#include "tftp.h"
#include "tftp_io.h"
#include "tftp_def.h"
#include "options.h"

#define S_BEGIN 0
#define S_SEND_REQ 1
#define S_SEND_ACK 2
#define S_SEND_OACK 3
#define S_SEND_DATA 4
#define S_WAIT_PACKET 5
#define S_REQ_RECEIVED 6
#define S_ACK_RECEIVED 7
#define S_OACK_RECEIVED 8
#define S_DATA_RECEIVED 9
#define S_ABORT 10
#define S_END 11

#define NB_BLOCK 2048

extern int tftp_cancel;
extern int tftp_prevent_sas;

/*
 * Receive a file. This is implemented as a state machine using a while loop
 * and a switch statement. Function flow is as follow:
 *  - sanity check
 *  - enter state machine
 *
 *     1) send request
 *     2) wait reply
 *          - if DATA packet, read it, send an acknoledge, goto 2
 *          - if OACK (option acknowledge) acknowledge this option, goto 2
 *          - if ERROR abort
 *          - if TIMEOUT goto previous state
 */
int tftp_receive_file(struct client_data *data)
{
     int state = S_SEND_REQ;    /* current state in the state machine */
     int timeout_state = state; /* what state should we go on when timeout */
     int result;
     long block_number = 0;
     long last_block_number = -1; /* block number of last block for multicast */
     int data_size;               /* size of data received */
     int sockfd = data->sockfd;   /* just to simplify calls */
     struct sockaddr_storage sa;  /* a copy of data.sa_peer */
     struct sockaddr_storage from;
     char from_str[SOCKADDR_PRINT_ADDR_LEN];
     int connected; /* 1 when sockfd is connected */
     struct tftphdr *tftphdr = (struct tftphdr *)data->data_buffer;
     FILE *fp = NULL; /* the local file pointer */
     int number_of_timeout = 0;
     int convert = 0; /* if true, do netascii conversion */

     int oacks = 0;     /* count OACK for improved error checking */
     int multicast = 0; /* set to 1 if multicast */
     int mcast_sockfd = 0;
     struct sockaddr_storage sa_mcast_group;
     int master_client = 0;
     unsigned int file_bitmap[NB_BLOCK];
     char string[MAXLEN];

     long prev_block_number = 0; /* needed to support netascii conversion */
     int temp = 0;

     data->file_size = 0;
     tftp_cancel = 0;

     memset(&from, 0, sizeof(from));
     memset(&sa_mcast_group, 0, sizeof(sa_mcast_group));
     memset(&file_bitmap, 0, sizeof(file_bitmap));

     /* make sure the socket is not connected */
     sa.ss_family = AF_UNSPEC;
     connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
     connected = 0;

     /* copy sa_peer structure */
     memcpy(&sa, &data->sa_peer, sizeof(sa));

     /* check to see if conversion is requiered */
     if (strcasecmp(data->tftp_options[OPT_MODE].value, "netascii") == 0)
          convert = 1;

     /* make sure the data buffer is SEGSIZE + 4 bytes */
     if (data->data_buffer_size != (SEGSIZE + 4))
     {
          data->data_buffer = realloc(data->data_buffer, SEGSIZE + 4);
          tftphdr = (struct tftphdr *)data->data_buffer;
          if (data->data_buffer == NULL)
          {
               fprintf(stderr, "tftp: memory allocation failure.\n");
               exit(1);
          }
          data->data_buffer_size = SEGSIZE + 4;
     }

     /* open the file for writing */
     if (data->fp != NULL)
     {
          fp = data->fp;
     }
     else if ((fp = fopen(data->local_file, "w")) == NULL)
     {
          fprintf(stderr, "tftp: can't open %s for writing.\n",
                  data->local_file);
          return ERR;
     }

     while (1)
     {
#ifdef DEBUG
          if (data->delay)
               usleep(data->delay * 1000);
#endif
          if (tftp_cancel)
          {
               if (from.ss_family == 0)
                    state = S_ABORT;
               else
               {
                    tftp_send_error(sockfd, &sa, EUNDEF, data->data_buffer,
                                    data->data_buffer_size, NULL);
                    if (data->trace)
                         fprintf(stderr, "sent ERROR <code: %d, msg: %s>\n",
                                 EUNDEF, tftp_errmsg[EUNDEF]);
                    state = S_ABORT;
               }
               tftp_cancel = 0;
          }

          switch (state)
          {
          case S_SEND_REQ:
               timeout_state = S_SEND_REQ;
               if (data->trace)
               {
                    opt_options_to_string(data->tftp_options, string, MAXLEN);
                    fprintf(stderr, "sent RRQ <file: %s, mode: %s <%s>>\n",
                            data->tftp_options[OPT_FILENAME].value,
                            data->tftp_options[OPT_MODE].value,
                            string);
               }

               sockaddr_set_port(&sa, sockaddr_get_port(&data->sa_peer));
               /* send request packet */
               if (tftp_send_request(sockfd, &sa, RRQ, data->data_buffer,
                                     data->data_buffer_size,
                                     data->tftp_options) == ERR)
                    state = S_ABORT;
               else
                    state = S_WAIT_PACKET;
               sockaddr_set_port(&sa, 0); /* must be set to 0 before the fist call to
                                   tftp_get_packet, but is was set before the
                                   call to tftp_send_request */
               break;
          case S_SEND_ACK:
               timeout_state = S_SEND_ACK;
               if (data->trace)
                    fprintf(stderr, "sent ACK <block: %ld>\n", block_number);
               tftp_send_ack(sockfd, &sa, block_number);
               /* if we just ACK the last block we are done */
               if (block_number == last_block_number)
                    state = S_END;
               else
                    state = S_WAIT_PACKET;
               break;
          case S_WAIT_PACKET:
               data_size = data->data_buffer_size;
               result = tftp_get_packet(sockfd, -1, NULL, &sa, &from, NULL,
                                        data->timeout, &data_size,
                                        data->data_buffer);
               /* Check that source port match */
               if ((sockaddr_get_port(&sa) != sockaddr_get_port(&from)) &&
                   ((result == GET_OACK) || (result == GET_ERROR) ||
                    (result == GET_DATA)))
               {
                    if (data->checkport)
                    {
                         result = GET_DISCARD;
                         fprintf(stderr, "source port mismatch\n");
                    }
                    else
                         fprintf(stderr, "source port mismatch, check bypassed");
               }

               switch (result)
               {
               case GET_TIMEOUT:
                    number_of_timeout++;
                    fprintf(stderr, "timeout: retrying...\n");
                    if (number_of_timeout > NB_OF_RETRY)
                         state = S_ABORT;
                    else
                         state = timeout_state;
                    break;
               case GET_OACK:
                    number_of_timeout = 0;
                    /* if the socket if not connected, connect it */
                    if (!connected)
                    {
                         connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
                         connected = 1;
                    }
                    state = S_OACK_RECEIVED;
                    break;
               case GET_ERROR:
                    fprintf(stderr, "tftp: error received from server <");
                    fwrite(tftphdr->th_msg, 1, data_size - 4 - 1, stderr);
                    fprintf(stderr, ">\n");

                    if (data->tftp_error_cb)
                    {
                         data->tftp_error_cb(ntohs(tftphdr->th_code),
                                             tftphdr->th_msg,
                                             data->tftp_error_ctx);
                    }

                    state = S_ABORT;
                    break;
               case GET_DATA:
                    number_of_timeout = 0;
                    /* if the socket if not connected, connect it */
                    if (!connected)
                    {
                         connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
                         connected = 1;
                    }
                    state = S_DATA_RECEIVED;
                    break;
               case GET_DISCARD:
                    /* consider discarded packet as timeout to make sure when don't
                       lock up when doing multicast transfer and routing is broken */
                    number_of_timeout++;
                    fprintf(stderr, "tftp: packet discard <%s:%d>.\n",
                            sockaddr_print_addr(&from, from_str, sizeof(from_str)),
                            sockaddr_get_port(&from));
                    if (number_of_timeout > NB_OF_RETRY)
                         state = S_ABORT;
                    break;
               case ERR:
                    fprintf(stderr, "tftp: unknown error.\n");
                    state = S_ABORT;
                    break;
               default:
                    fprintf(stderr, "tftp: abnormal return value %d.\n",
                            result);
               }
               break;
          case S_OACK_RECEIVED:
               oacks++;

               /* Normally we shouldn't receive more than one OACK
                  in non-multicast mode. */
               if (oacks > 1)
               {
                    tftp_send_error(sockfd, &sa, EUNDEF, data->data_buffer,
                                    data->data_buffer_size, NULL);
                    fprintf(stderr, "tftp: unexpected OACK\n");
                    state = S_ABORT;
                    break;
               }

               /* clean the tftp_options structure */
               memcpy(data->tftp_options_reply, tftp_default_options,
                      sizeof(tftp_default_options));
               /*
                * look in the returned string for tsize, timeout, blksize
                * or multicast
                */
               opt_disable_options(data->tftp_options_reply, NULL);
               opt_parse_options(data->data_buffer, data_size,
                                 data->tftp_options_reply);
               if (data->trace)
                    fprintf(stderr, "received OACK <");
               /* blksize: resize the buffer please */
               if ((result = opt_get_blksize(data->tftp_options_reply)) > -1)
               {
                    if (data->trace)
                         fprintf(stderr, "blksize: %d, ", result);

                    data->data_buffer = realloc(data->data_buffer,
                                                result + 4);
                    tftphdr = (struct tftphdr *)data->data_buffer;
                    if (data->data_buffer == NULL)
                    {
                         fprintf(stderr,
                                 "tftp: memory allocation failure.\n");
                         exit(1);
                    }
                    data->data_buffer_size = result + 4;
               }

               if (data->trace)
                    fprintf(stderr, "\b\b>\n");
               if ((multicast && master_client) || (!multicast))
                    state = S_SEND_ACK;
               else
                    state = S_WAIT_PACKET;
               break;
          case S_DATA_RECEIVED:
               if ((multicast && master_client) || (!multicast))
                    timeout_state = S_SEND_ACK;
               else
                    timeout_state = S_WAIT_PACKET;

               block_number = tftp_rollover_blocknumber(
                   ntohs(tftphdr->th_block), prev_block_number, 0);
               if (data->trace)
                    fprintf(stderr, "received DATA <block: %ld, size: %d>\n",
                            block_number, data_size - 4);

               if (data->tftp_fetch_data_received_cbk != NULL)
               {
                    data->tftp_fetch_data_received_cbk(data_size - 4,
                                                       data->tftp_fetch_data_received_ctx);
               }

               if (tftp_file_write(fp, tftphdr->th_data, data->data_buffer_size - 4, block_number,
                                   data_size - 4, convert, &prev_block_number, &temp) != data_size - 4)
               {

                    fprintf(stderr, "tftp: error writing to file %s\n",
                            data->local_file);
                    tftp_send_error(sockfd, &sa, ENOSPACE, data->data_buffer,
                                    data->data_buffer_size, NULL);
                    state = S_ABORT;
                    break;
               }
               data->file_size += data_size;
               /* Record the block number of the last block. The last block
                  is the one with less data than the transfer block size */
               if (data_size < data->data_buffer_size)
                    last_block_number = block_number;

               state = S_SEND_ACK;
               break;
          case S_END:
          case S_ABORT:
               /* close file */
               if (fp && data->fp == NULL)
                    fclose(fp);
               /* close multicast socket */
               if (mcast_sockfd)
                    close(mcast_sockfd);
               /* return proper error code */
               if (state == S_END)
                    return OK;
               else
                    fprintf(stderr, "tftp: aborting\n");
          default:
               return ERR;
          }
     }
}

/*
 * Send a file. This is implemented as a state machine using a while loop
 * and a switch statement. Function flow is as follow:
 *  - sanity check
 *  - enter state machine
 *
 *     1) send request
 *     2) wait replay
 *          - if ACK, goto 3
 *          - if OACK (option acknowledge) acknowledge this option, goto 2
 *          - if ERROR abort
 *          - if TIMEOUT goto previous state
 *     3) send data, goto 2
 */
int tftp_send_file(struct client_data *data)
{
     int state = S_SEND_REQ;    /* current state in the state machine */
     int timeout_state = state; /* what state should we go on when timeout */
     int result;
     long block_number = 0;
     long last_requested_block = -1;
     long last_block = -1;
     int data_size;              /* size of data received */
     int sockfd = data->sockfd;  /* just to simplify calls */
     struct sockaddr_storage sa; /* a copy of data.sa_peer */
     struct sockaddr_storage from;
     char from_str[SOCKADDR_PRINT_ADDR_LEN];
     int connected; /* 1 when sockfd is connected */
     struct tftphdr *tftphdr = (struct tftphdr *)data->data_buffer;
     FILE *fp; /* the local file pointer */
     int number_of_timeout = 0;
     struct stat file_stat;
     int convert = 0; /* if true, do netascii conversion */
     char string[MAXLEN];

     long prev_block_number = 0; /* needed to support netascii conversion */
     long prev_file_pos = 0;
     int temp = 0;

     data->file_size = 0;
     tftp_cancel = 0;
     memset(&from, 0, sizeof(from));

     /* make sure the socket is not connected */
     sa.ss_family = AF_UNSPEC;
     connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
     connected = 0;

     /* copy sa_peer structure */
     memcpy(&sa, &data->sa_peer, sizeof(sa));

     /* check to see if conversion is requiered */
     if (strcasecmp(data->tftp_options[OPT_MODE].value, "netascii") == 0)
          convert = 1;

     /* make sure the data buffer is SEGSIZE + 4 bytes */
     if (data->data_buffer_size != (SEGSIZE + 4))
     {
          data->data_buffer = realloc(data->data_buffer, SEGSIZE + 4);
          tftphdr = (struct tftphdr *)data->data_buffer;
          if (data->data_buffer == NULL)
          {
               fprintf(stderr, "tftp: memory allocation failure.\n");
               exit(1);
          }
          data->data_buffer_size = SEGSIZE + 4;
     }

     /* open the file for reading */
     if (data->fp != NULL)
     {
          fp = data->fp;
     }
     else if ((fp = fopen(data->local_file, "r")) == NULL)
     {
          fprintf(stderr, "tftp: can't open %s for reading.\n",
                  data->local_file);
          return ERR;
     }

     /* When sending a file with the tsize argument, we shall
        put the file size as argument */
     fstat(fileno(fp), &file_stat);

     while (1)
     {
#ifdef DEBUG
          if (data->delay)
               usleep(data->delay * 1000);
#endif
          if (tftp_cancel)
          {
               /* Make sure we know the peer's address */
               if (from.ss_family == 0)
                    state = S_ABORT;
               else
               {
                    tftp_send_error(sockfd, &sa, EUNDEF, data->data_buffer,
                                    data->data_buffer_size, NULL);

                    if (data->trace)
                         fprintf(stderr, "sent ERROR <code: %d, msg: %s>\n",
                                 EUNDEF, tftp_errmsg[EUNDEF]);
                    state = S_ABORT;
               }
               tftp_cancel = 0;
          }

          switch (state)
          {
          case S_SEND_REQ:
               timeout_state = S_SEND_REQ;
               if (data->trace)
               {
                    opt_options_to_string(data->tftp_options, string, MAXLEN);
                    fprintf(stderr, "sent WRQ <file: %s, mode: %s <%s>>\n",
                            data->tftp_options[OPT_FILENAME].value,
                            data->tftp_options[OPT_MODE].value,
                            string);
               }

               sockaddr_set_port(&sa, sockaddr_get_port(&data->sa_peer));
               /* send request packet */
               if (tftp_send_request(sockfd, &sa, WRQ, data->data_buffer,
                                     data->data_buffer_size,
                                     data->tftp_options) == ERR)
                    state = S_ABORT;
               else
                    state = S_WAIT_PACKET;
               sockaddr_set_port(&sa, 0); /* must be set to 0 before the fist call to
                                   tftp_get_packet, but is was set before the
                                   call to tftp_send_request */
               break;
          case S_SEND_DATA:
               timeout_state = S_SEND_DATA;

               data_size = tftp_file_read(fp, tftphdr->th_data, data->data_buffer_size - 4, block_number,
                                          convert, &prev_block_number, &prev_file_pos, &temp);
               data_size += 4; /* need to consider tftp header */

               if (feof(fp))
                    last_block = block_number;
               tftp_send_data(sockfd, &sa, block_number + 1,
                              data_size, data->data_buffer);
               data->file_size += data_size;
               if (data->trace)
                    fprintf(stderr, "sent DATA <block: %ld, size: %d>\n",
                            block_number + 1, data_size - 4);
               state = S_WAIT_PACKET;
               break;
          case S_WAIT_PACKET:
               data_size = data->data_buffer_size;
               result = tftp_get_packet(sockfd, -1, NULL, &sa, &from, NULL,
                                        data->timeout, &data_size,
                                        data->data_buffer);
               /* check that source port match */
               if (sockaddr_get_port(&sa) != sockaddr_get_port(&from))
               {
                    if ((data->checkport) &&
                        ((result == GET_ACK) || (result == GET_OACK) ||
                         (result == GET_ERROR)))
                         result = GET_DISCARD;
                    else
                         fprintf(stderr, "source port mismatch, check bypassed");
               }

               switch (result)
               {
               case GET_TIMEOUT:
                    number_of_timeout++;
                    fprintf(stderr, "timeout: retrying...\n");
                    if (number_of_timeout > NB_OF_RETRY)
                         state = S_ABORT;
                    else
                         state = timeout_state;
                    break;
               case GET_ACK:
                    number_of_timeout = 0;
                    /* if the socket if not connected, connect it */
                    if (!connected)
                    {
                         // connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
                         connected = 1;
                    }
                    block_number = tftp_rollover_blocknumber(
                        ntohs(tftphdr->th_block), prev_block_number, 0);

                    /* if turned on, check whether the block request isn't already fulfilled */
                    if (tftp_prevent_sas)
                    {
                         if (last_requested_block >= block_number)
                         {
                              if (data->trace)
                                   fprintf(stderr, "received duplicated ACK <block: %ld >= %ld>\n",
                                           last_requested_block, block_number);
                              break;
                         }
                         else
                              last_requested_block = block_number;
                    }

                    if (data->trace)
                         fprintf(stderr, "received ACK <block: %ld>\n",
                                 block_number);
                    if ((last_block != -1) && (block_number > last_block))
                    {
                         state = S_END;
                         break;
                    }
                    state = S_SEND_DATA;
                    break;
               case GET_OACK:
                    number_of_timeout = 0;
                    /* if the socket if not connected, connect it */
                    if (!connected)
                    {
                         // connect(sockfd, (struct sockaddr *)&sa, sizeof(sa));
                         connected = 1;
                    }
                    state = S_OACK_RECEIVED;
                    break;
               case GET_ERROR:
                    fprintf(stderr, "tftp: error received from server <");
                    fwrite(tftphdr->th_msg, 1, data_size - 4 - 1, stderr);
                    fprintf(stderr, ">\n");

                    if (data->tftp_error_cb)
                    {
                         data->tftp_error_cb(ntohs(tftphdr->th_code),
                                             tftphdr->th_msg,
                                             data->tftp_error_ctx);
                    }

                    state = S_ABORT;
                    break;
               case GET_DISCARD:
                    /* consider discarded packet as timeout to make sure when don't lock up
                       if routing is broken */
                    number_of_timeout++;
                    fprintf(stderr, "tftp: packet discard <%s:%d>.\n",
                            sockaddr_print_addr(&from, from_str, sizeof(from_str)),
                            sockaddr_get_port(&from));
                    if (number_of_timeout > NB_OF_RETRY)
                         state = S_ABORT;
                    break;
               case ERR:
                    fprintf(stderr, "tftp: unknown error.\n");
                    state = S_ABORT;
                    break;
               default:
                    fprintf(stderr, "tftp: abnormal return value %d.\n",
                            result);
               }
               break;
          case S_OACK_RECEIVED:
               /* clean the tftp_options structure */
               memcpy(data->tftp_options_reply, tftp_default_options,
                      sizeof(tftp_default_options));
               /*
                * look in the returned string for tsize, timeout, blksize or
                * multicast
                */
               opt_disable_options(data->tftp_options_reply, NULL);
               opt_parse_options(data->data_buffer, data_size,
                                 data->tftp_options_reply);
               if (data->trace)
                    fprintf(stderr, "received OACK <");
               /* blksize: resize the buffer please */
               if ((result = opt_get_blksize(data->tftp_options_reply)) > -1)
               {
                    if (data->trace)
                         fprintf(stderr, "blksize: %d, ", result);
                    data->data_buffer = realloc(data->data_buffer,
                                                result + 4);
                    tftphdr = (struct tftphdr *)data->data_buffer;
                    if (data->data_buffer == NULL)
                    {
                         fprintf(stderr,
                                 "tftp: memory allocation failure.\n");
                         exit(1);
                    }
                    data->data_buffer_size = result + 4;
               }

               if (data->trace)
                    fprintf(stderr, "\b\b>\n");
               state = S_SEND_DATA;
               break;
          case S_END:
               if (fp && data->fp == NULL)
                    fclose(fp);
               return OK;
               break;
          case S_ABORT:
               if (fp && data->fp == NULL)
                    fclose(fp);
               fprintf(stderr, "tftp: aborting\n");
          default:
               return ERR;
          }
     }
}
