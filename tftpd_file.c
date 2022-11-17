// Modified by Kollins G. Lima <kgl2@cin.ufpe.br>
/* hey emacs! -*- Mode: C; c-file-style: "k&r"; indent-tabs-mode: nil -*- */
/*
 * tftpd_file.c
 *    state machine for file transfer on the server side
 *
 * $Id: tftpd_file.c,v 1.51 2004/02/18 02:21:47 jp Exp $
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "tftpd.h"
#include "tftp_io.h"
#include "tftp_def.h"
#include "atftp_logger.h"
#include "options.h"
#ifdef HAVE_PCRE
#include "tftpd_pcre.h"
#endif

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

/* read only variables unless for the main thread, at initialisation */
extern char directory[MAXLEN];
/* read only except for the main thread */
// extern int tftpd_cancel;
extern int tftpd_prevent_sas;

#ifdef HAVE_PCRE
extern tftpd_pcre_self_t *pcre_top;
#endif

/*
 * Receive a file. It is implemented as a state machine using a while loop
 * and a switch statement. Function flow is as follow:
 *  - sanity check
 *  - check client's request
 *  - enter state machine
 *
 *     1) send a ACK or OACK
 *     2) wait replay
 *          - if DATA packet, read it, send an acknoledge, goto 2
 *          - if ERROR abort
 *          - if TIMEOUT goto previous state
 */
int tftpd_receive_file(struct thread_data *data)
{
     int state = S_BEGIN;
     int timeout_state = state;
     int result;
     long block_number = 0;
     int data_size;
     int sockfd = data->sockfd;
     struct sockaddr_storage *sa = &data->client_info->client;
     struct sockaddr_storage from;
     char addr_str[SOCKADDR_PRINT_ADDR_LEN];
     struct tftphdr *tftphdr = (struct tftphdr *)data->data_buffer;
     FILE *fp = NULL;
     char filename[MAXLEN];
     char string[MAXLEN];
     int timeout = data->timeout;
     int number_of_timeout = 0;
     int all_blocks_received = 0; /* temporary kludge */
     int convert = 0;             /* if true, do netascii conversion */

     long prev_block_number = 0; /* needed to support netascii conversion */
     int temp = 0;

     /* file name verification */
     Strncpy(filename, data->tftp_options[OPT_FILENAME].value,
             MAXLEN);

     /*
      *  blksize option, must be the last option evaluated,
      *  because data->data_buffer_size may be modified here,
      *  and may be smaller than the buffer containing options
      */
     if ((result = opt_get_blksize(data->tftp_options)) > -1)
     {
          /*
           *  If we receive more options, we have to make sure our buffer for
           *  the OACK is not too small.  Use the string representation of
           *  the options here for simplicity, which puts us on the save side.
           *  FIXME: Use independent buffers for OACK and data.
           */
          opt_options_to_string(data->tftp_options, string, MAXLEN);
          if ((result < strlen(string) - 2) || (result > 65464))
          {
               atftp_logger(LOG_NOTICE, "options <%s> require roughly a blksize of %d for the OACK.",
                            string, strlen(string) - 2);
               tftp_send_error(sockfd, sa, EOPTNEG, data->data_buffer,
                               data->data_buffer_size, NULL);
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EOPTNEG,
                                 tftp_errmsg[EOPTNEG]);
               return ERR;
          }

          data->data_buffer_size = result + 4;
          data->data_buffer = realloc(data->data_buffer, data->data_buffer_size);

          if (data->data_buffer == NULL)
          {
               atftp_logger(LOG_ERR, "memory allocation failure");
               return ERR;
          }
          tftphdr = (struct tftphdr *)data->data_buffer;

          if (data->data_buffer == NULL)
          {
               tftp_send_error(sockfd, sa, ENOSPACE, data->data_buffer,
                               data->data_buffer_size, NULL);
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", ENOSPACE,
                                 tftp_errmsg[ENOSPACE]);
               return ERR;
          }
          opt_set_blksize(result, data->tftp_options);
          atftp_logger(LOG_DEBUG, "blksize option -> %d", result);
     }

     /* that's it, we start receiving the file */
     while (1)
     {
          if (*(data->tftpd_cancel))
          {
               atftp_logger(LOG_DEBUG, "thread cancelled");
               tftp_send_error(sockfd, sa, EUNDEF, data->data_buffer,
                               data->data_buffer_size, NULL);
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EUNDEF,
                                 tftp_errmsg[EUNDEF]);
               state = S_ABORT;
          }

          switch (state)
          {
          case S_BEGIN:
               /* Did the client request RFC1350 options ?*/
               if (opt_support_options(data->tftp_options))
                    state = S_SEND_OACK;
               else
                    state = S_SEND_ACK;
               break;
          case S_SEND_ACK:
               timeout_state = state;
               tftp_send_ack(sockfd, sa, block_number);
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "sent ACK <block: %ld>", block_number);
               if (all_blocks_received)
                    state = S_END;
               else
                    state = S_WAIT_PACKET;
               break;
          case S_SEND_OACK:
               timeout_state = state;
               tftp_send_oack(sockfd, sa, data->tftp_options,
                              data->data_buffer, data->data_buffer_size);
               opt_options_to_string(data->tftp_options, string, MAXLEN);
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "sent OACK <%s>", string);
               state = S_WAIT_PACKET;
               break;
          case S_WAIT_PACKET:
               data_size = data->data_buffer_size;
               result = tftp_get_packet(sockfd, -1, NULL, sa, &from, NULL,
                                        timeout, &data_size, data->data_buffer);

               switch (result)
               {
               case GET_TIMEOUT:
                    number_of_timeout++;
                    if (number_of_timeout > NB_OF_RETRY)
                    {
                         atftp_logger(LOG_INFO, "client (%s) not responding",
                                      sockaddr_print_addr(&data->client_info->client,
                                                          addr_str, sizeof(addr_str)));
                         state = S_END;
                    }
                    else
                    {
                         atftp_logger(LOG_WARNING, "timeout: retrying...");
                         state = timeout_state;
                    }
                    break;
               case GET_ERROR:
                    /*
                     * This does not work correctly if load balancers are
                     * employed on one side of a multihomed host. ie, the
                     * tftp RRQ goes through the load balancer and has the
                     * source ports changed while the multicast traffic
                     * bypasses the load balancers.
                     *
                     * **** It is not compliant with RFC1350 to skip this
                     * **** test since the port number is the TID. Use this
                     * **** only if you know what you're doing.
                     */
                    if (sockaddr_get_port(sa) != sockaddr_get_port(&from))
                    {
                         if (data->checkport)
                         {
                              atftp_logger(LOG_WARNING, "packet discarded <%s>",
                                           sockaddr_print_addr(&from, addr_str,
                                                               sizeof(addr_str)));
                              break;
                         }
                         else
                              atftp_logger(LOG_WARNING, "source port mismatch, check bypassed");
                    }
                    Strncpy(string, tftphdr->th_msg, sizeof(string));
                    if (data->trace)
                         atftp_logger(LOG_DEBUG, "received ERROR <code: %d, msg: %s>",
                                      ntohs(tftphdr->th_code), string);
                    state = S_ABORT;
                    break;
               case GET_DATA:
                    /* Check that source port match */
                    if (sockaddr_get_port(sa) != sockaddr_get_port(&from))
                    {
                         if (data->checkport)
                         {
                              atftp_logger(LOG_WARNING, "packet discarded <%s>",
                                           sockaddr_print_addr(&from, addr_str,
                                                               sizeof(addr_str)));
                              break;
                         }
                         else
                              atftp_logger(LOG_WARNING, "source port mismatch, check bypassed");
                    }
                    number_of_timeout = 0;
                    state = S_DATA_RECEIVED;
                    break;
               case GET_DISCARD:
                    /* FIXME: should we increment number_of_timeout */
                    atftp_logger(LOG_WARNING, "packet discarded <%s>",
                                 sockaddr_print_addr(&from, addr_str,
                                                     sizeof(addr_str)));
                    break;
               case ERR:
                    atftp_logger(LOG_ERR, "%s: %d: recvfrom: %s",
                                 __FILE__, __LINE__, strerror(errno));
                    state = S_ABORT;
                    break;
               default:
                    atftp_logger(LOG_ERR, "%s: %d: abnormal return value %d",
                                 __FILE__, __LINE__, result);
                    state = S_ABORT;
               }
               break;
          case S_DATA_RECEIVED:
               if (fp == NULL)
               {
                    /* Open the file for writing. */
                    if (data->open_file_cb == NULL)
                    {
                         fp = fopen(filename, "w");
                    }
                    else
                    {
                         data->open_file_cb(data->section_handler_ptr, &fp, filename, "w", NULL, data->open_file_ctx);
                    }
                    if (fp == NULL)
                    {
                         char *custom_error_msg = NULL;
                         get_error_msg(data->section_handler_ptr, &custom_error_msg);

                         /* Can't create the file. */
                         atftp_logger(LOG_INFO, "Can't open %s for writing", filename);
                         tftp_send_error(sockfd, sa, EACCESS,
                                         data->data_buffer,
                                         data->data_buffer_size,
                                         custom_error_msg);

                         if (custom_error_msg != NULL)
                         {
                              free(custom_error_msg);
                         }

                         if (data->trace)
                              atftp_logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EACCESS,
                                           tftp_errmsg[EACCESS]);
                         return ERR;
                    }
               }

               /* We need to seek to the right place in the file */
               block_number = tftp_rollover_blocknumber(
                   ntohs(tftphdr->th_block), prev_block_number, 0);
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "received DATA <block: %ld, size: %d>",
                                 block_number, data_size - 4);

               if (tftp_file_write(fp, tftphdr->th_data, data->data_buffer_size - 4, block_number,
                                   data_size - 4, convert, &prev_block_number, &temp) != data_size - 4)
               {
                    atftp_logger(LOG_ERR, "%s: %d: error writing to file %s",
                                 __FILE__, __LINE__, filename);
                    tftp_send_error(sockfd, sa, ENOSPACE, data->data_buffer,
                                    data->data_buffer_size, NULL);
                    if (data->trace)
                         atftp_logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>",
                                      ENOSPACE, tftp_errmsg[ENOSPACE]);
                    state = S_ABORT;
                    break;
               }
               if (data_size < data->data_buffer_size)
                    all_blocks_received = 1;
               else
                    all_blocks_received = 0;
               state = S_SEND_ACK;
               break;
          case S_END:
               if (fp != NULL)
               {
                    if (data->close_file_cb == NULL)
                    {
                         fclose(fp);
                    }
                    else
                    {
                         data->close_file_cb(data->section_handler_ptr, fp, data->close_file_ctx);
                    }
               }
               return OK;
          case S_ABORT:
               if (fp != NULL)
               {
                    if (data->close_file_cb == NULL)
                    {
                         fclose(fp);
                    }
                    else
                    {
                         data->close_file_cb(data->section_handler_ptr, fp, data->close_file_ctx);
                    }
               }
               return ERR;
          default:
               if (fp != NULL)
               {
                    if (data->close_file_cb == NULL)
                    {
                         fclose(fp);
                    }
                    else
                    {
                         data->close_file_cb(data->section_handler_ptr, fp, data->close_file_ctx);
                    }
               }
               atftp_logger(LOG_ERR, "%s: %d: tftpd_file.c: huh?",
                            __FILE__, __LINE__);
               return ERR;
          }
     }
}

/*
 * Send a file. It is implemented as a state machine using a while loop
 * and a switch statement. Function flow is as follow:
 *  - sanity check
 *  - check client's request
 *  - enter state machine
 *
 *     1) send a DATA or OACK
 *     2) wait replay
 *          - if ACK, goto 3
 *          - if ERROR abort
 *          - if TIMEOUT goto previous state
 *     3) send data, goto 2
 */
int tftpd_send_file(struct thread_data *data)
{
     int state = S_BEGIN;
     int timeout_state = state;
     int result;
     long block_number = 0;
     long last_block = -1;
     int data_size;
     struct sockaddr_storage *sa = &data->client_info->client;
     struct sockaddr_storage from;
     char addr_str[SOCKADDR_PRINT_ADDR_LEN];
     int sockfd = data->sockfd;
     struct tftphdr *tftphdr = (struct tftphdr *)data->data_buffer;
     FILE *fp;
     char filename[MAXLEN];
     char string[MAXLEN];
     int timeout = data->timeout;
     int number_of_timeout = 0;
     struct stat file_stat;
     size_t file_size = 0;
     int convert = 0; /* if true, do netascii conversion */

     struct client_info *client_info = data->client_info;
     struct tftp_opt options[OPT_NUMBER];

     long prev_block_number = 0; /* needed to support netascii conversion */
     long prev_file_pos = 0;
     int temp = 0;

     long prev_sent_block = -1;
     int prev_sent_count = 0;
     int prev_ack_count = 0;
     int curr_sent_count = 0;

     /* look for mode option */
     if (strcasecmp(data->tftp_options[OPT_MODE].value, "netascii") == 0)
     {
          convert = 1;
          atftp_logger(LOG_DEBUG, "will do netascii conversion");
     }

     /* file name verification */
     Strncpy(filename, data->tftp_options[OPT_FILENAME].value,
             MAXLEN);

     /* verify that the requested file exist */
     if (data->open_file_cb == NULL)
     {
          fp = fopen(filename, "r");
     }
     else
     {
          data->open_file_cb(data->section_handler_ptr, &fp, filename, "r",
                             &file_size, data->open_file_ctx);
     }

     if (fp == NULL)
     {
          char *custom_error_msg = NULL;
          get_error_msg(data->section_handler_ptr, &custom_error_msg);

          tftp_send_error(sockfd, sa, ENOTFOUND, data->data_buffer,
                          data->data_buffer_size,
                          custom_error_msg);

          if (custom_error_msg != NULL)
          {
               free(custom_error_msg);
          }

          atftp_logger(LOG_INFO, "File %s not found", filename);
          if (data->trace)
               atftp_logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", ENOTFOUND,
                            tftp_errmsg[ENOTFOUND]);
          return ERR;
     }

     /* To return the size of the file with tsize argument */
     if (file_size == 0)
     {
          if (fstat(fileno(fp), &file_stat) == 0)
          {
               file_size = file_stat.st_size;
          }
     }

     /*
      *  blksize option, must be the last option evaluated,
      *  because data->data_buffer_size may be modified here,
      *  and may be smaller than the buffer containing options
      */
     if ((result = opt_get_blksize(data->tftp_options)) > -1)
     {
          /*
           *  If we receive more options, we have to make sure our buffer for
           *  the OACK is not too small.  Use the string representation of
           *  the options here for simplicity, which puts us on the save side.
           *  FIXME: Use independent buffers for OACK and data.
           */
          opt_options_to_string(data->tftp_options, string, MAXLEN);
          if ((result < strlen(string) - 2) || (result > 65464))
          {
               atftp_logger(LOG_NOTICE, "options <%s> require roughly a blksize of %d for the OACK.",
                            string, strlen(string) - 2);
               tftp_send_error(sockfd, sa, EOPTNEG, data->data_buffer,
                               data->data_buffer_size, NULL);
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EOPTNEG,
                                 tftp_errmsg[EOPTNEG]);

               if (data->close_file_cb != NULL)
               {
                    data->close_file_cb(data->section_handler_ptr, fp, data->close_file_ctx);
               }
               else
               {
                    fclose(fp);
               }
               return ERR;
          }

          data->data_buffer_size = result + 4;
          data->data_buffer = realloc(data->data_buffer, data->data_buffer_size);

          if (data->data_buffer == NULL)
          {
               atftp_logger(LOG_ERR, "memory allocation failure");
               if (data->close_file_cb != NULL)
               {
                    data->close_file_cb(data->section_handler_ptr, fp, data->close_file_ctx);
               }
               else
               {
                    fclose(fp);
               }
               return ERR;
          }
          tftphdr = (struct tftphdr *)data->data_buffer;

          if (data->data_buffer == NULL)
          {
               tftp_send_error(sockfd, sa, ENOSPACE, data->data_buffer,
                               data->data_buffer_size, NULL);
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", ENOSPACE,
                                 tftp_errmsg[ENOSPACE]);
               if (data->close_file_cb != NULL)
               {
                    data->close_file_cb(data->section_handler_ptr, fp, data->close_file_ctx);
               }
               else
               {
                    fclose(fp);
               }
               return ERR;
          }
          opt_set_blksize(result, data->tftp_options);
          atftp_logger(LOG_INFO, "blksize option -> %d", result);
     }

     /* Verify that the file can be sent in MAXBLOCKS blocks of BLKSIZE octets */
     if ((file_size / (data->data_buffer_size - 4)) > MAXBLOCKS)
     {
          tftp_send_error(sockfd, sa, EUNDEF, data->data_buffer,
                          data->data_buffer_size, NULL);
          atftp_logger(LOG_NOTICE, "Requested file too big, increase BLKSIZE");
          atftp_logger(LOG_NOTICE, "Only %d blocks of %d bytes can be served via multicast", MAXBLOCKS, data->data_buffer_size);
          if (data->trace)
               atftp_logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s>", EUNDEF,
                            tftp_errmsg[EUNDEF]);
          if (data->close_file_cb != NULL)
          {
               data->close_file_cb(data->section_handler_ptr, fp, data->close_file_ctx);
          }
          else
          {
               fclose(fp);
          }
          return ERR;
     }

     /* copy options to local structure, used when falling back a client to slave */
     memcpy(options, data->tftp_options, sizeof(options));

     /* That's it, ready to send the file */
     while (1)
     {
          if (*(data->tftpd_cancel))
          {
               /* Send error to all client */
               atftp_logger(LOG_DEBUG, "thread cancelled");
               do
               {
                    tftpd_clientlist_done(data->handler, data, client_info, NULL);
                    tftp_send_error(sockfd, &client_info->client,
                                    EUNDEF, data->data_buffer,
                                    data->data_buffer_size, NULL);
                    if (data->trace)
                    {
                         atftp_logger(LOG_DEBUG, "sent ERROR <code: %d, msg: %s> to %s", EUNDEF,
                                      tftp_errmsg[EUNDEF],
                                      sockaddr_print_addr(&client_info->client,
                                                          addr_str, sizeof(addr_str)));
                    }
               } while (tftpd_clientlist_next(data->handler, data, &client_info) == 1);
               state = S_ABORT;
          }

          switch (state)
          {
          case S_BEGIN:
               if (opt_support_options(data->tftp_options))
                    state = S_SEND_OACK;
               else
                    state = S_SEND_DATA;
               break;
          case S_SEND_OACK:
               timeout_state = state;
               opt_options_to_string(data->tftp_options, string, MAXLEN);
               if (data->trace)
                    atftp_logger(LOG_DEBUG, "sent OACK <%s>", string);
               tftp_send_oack(sockfd, sa, data->tftp_options,
                              data->data_buffer, data->data_buffer_size);
               state = S_WAIT_PACKET;
               break;
          case S_SEND_DATA:
               timeout_state = state;

               data_size = tftp_file_read(fp, tftphdr->th_data, data->data_buffer_size - 4, block_number,
                                          convert, &prev_block_number, &prev_file_pos, &temp);
               data_size += 4; /* need to consider tftp header */

               /* record the last block number */
               if (feof(fp))
                    last_block = block_number;

               tftp_send_data(sockfd, sa, block_number + 1,
                              data_size, data->data_buffer);

               if (data->trace)
                    atftp_logger(LOG_DEBUG, "sent DATA <block: %ld, size %d>",
                                 block_number + 1, data_size - 4);
               state = S_WAIT_PACKET;
               break;
          case S_WAIT_PACKET:
               data_size = data->data_buffer_size;
               result = tftp_get_packet(sockfd, -1, NULL, sa, &from, NULL,
                                        timeout, &data_size, data->data_buffer);
               switch (result)
               {
               case GET_TIMEOUT:
                    number_of_timeout++;

                    if (number_of_timeout > NB_OF_RETRY)
                    {
                         atftp_logger(LOG_INFO, "client (%s) not responding",
                                      sockaddr_print_addr(&client_info->client,
                                                          addr_str, sizeof(addr_str)));
                         state = S_END;
                    }
                    else
                    {
                         atftp_logger(LOG_WARNING, "timeout: retrying...");
                         state = timeout_state;
                    }
                    break;
               case GET_ACK:
                    /* handle case where packet come from un unexpected client */

                    /* check that the packet is from the current client */
                    if (sockaddr_get_port(sa) != sockaddr_get_port(&from))
                    {
                         if (data->checkport)
                         {
                              atftp_logger(LOG_WARNING, "packet discarded <%s:%d>",
                                           sockaddr_print_addr(&from, addr_str,
                                                               sizeof(addr_str)),
                                           sockaddr_get_port(&from));
                              break;
                         }
                         else
                              atftp_logger(LOG_WARNING,
                                           "source port mismatch, check bypassed");
                    }

                    /* The ACK is from the current client */
                    number_of_timeout = 0;

                    block_number = tftp_rollover_blocknumber(
                        ntohs(tftphdr->th_block), prev_block_number, 0);
                    if (data->trace)
                         atftp_logger(LOG_DEBUG, "received ACK <block: %ld>",
                                      block_number);

                    /* Now check the ACK number and possibly ignore the request */

                    /* here comes the ACK again */
                    if (prev_sent_block == block_number)
                    {
                         /* drop if number of ACKs == times of previous block sending */
                         if (++prev_ack_count == prev_sent_count)
                         {
                              atftp_logger(LOG_DEBUG, "ACK count (%d) == previous block transmission count -> dropping ACK", prev_ack_count);
                              break;
                         }
                         /* else resend the block */
                         atftp_logger(LOG_DEBUG, "resending block %d", block_number + 1);
                    }
                    /* received ACK to sent block -> move on to next block */
                    else if (prev_sent_block < block_number)
                    {
                         prev_sent_block = block_number;
                         prev_sent_count = curr_sent_count;
                         curr_sent_count = 0;
                         prev_ack_count = 1;
                    }
                    /* nor previous nor current block number -> ignore it completely */
                    else
                    {
                         atftp_logger(LOG_DEBUG, "ignoring ACK %d", block_number);
                         break;
                    }

                    if ((last_block != -1) && (block_number > last_block))
                    {
                         state = S_END;
                         break;
                    }

                    curr_sent_count++;
                    state = S_SEND_DATA;
                    break;
               case GET_ERROR:
                    /* handle case where packet come from un unexpected client */

                    /* check that the packet is from the current client */
                    if (sockaddr_get_port(sa) != sockaddr_get_port(&from))
                    {
                         if (data->checkport)
                         {
                              atftp_logger(LOG_WARNING, "packet discarded <%s>",
                                           sockaddr_print_addr(&from, addr_str,
                                                               sizeof(addr_str)));
                              break;
                         }
                         else
                              atftp_logger(LOG_WARNING,
                                           "source port mismatch, check bypassed");
                    }

                    /* Got an ERROR from the current master client */
                    Strncpy(string, tftphdr->th_msg, sizeof(string));
                    if (data->trace)
                         atftp_logger(LOG_DEBUG, "received ERROR <code: %d, msg: %s>",
                                      ntohs(tftphdr->th_code), string);

                    state = S_ABORT;
                    break;
               case GET_DISCARD:
                    /* FIXME: should we increment number_of_timeout */
                    atftp_logger(LOG_WARNING, "packet discarded <%s>",
                                 sockaddr_print_addr(&from, addr_str,
                                                     sizeof(addr_str)));
                    break;
               case ERR:
                    atftp_logger(LOG_ERR, "%s: %d: recvfrom: %s",
                                 __FILE__, __LINE__, strerror(errno));
                    state = S_ABORT;
                    break;
               default:
                    atftp_logger(LOG_ERR, "%s: %d: abnormal return value %d",
                                 __FILE__, __LINE__, result);
               }
               break;
          case S_END:
               atftp_logger(LOG_DEBUG, "End of transfer");
               if (data->close_file_cb != NULL)
               {
                    data->close_file_cb(data->section_handler_ptr, fp, data->close_file_ctx);
               }
               else
               {
                    fclose(fp);
               }
               return OK;
               break;
          case S_ABORT:
               atftp_logger(LOG_DEBUG, "Aborting transfer");
               if (data->close_file_cb != NULL)
               {
                    data->close_file_cb(data->section_handler_ptr, fp, data->close_file_ctx);
               }
               else
               {
                    fclose(fp);
               }
               return ERR;
          default:
               if (data->close_file_cb != NULL)
               {
                    data->close_file_cb(data->section_handler_ptr, fp, data->close_file_ctx);
               }
               else
               {
                    fclose(fp);
               }
               atftp_logger(LOG_ERR, "%s: %d: abnormal condition",
                            __FILE__, __LINE__);
               return ERR;
          }
     }
}
