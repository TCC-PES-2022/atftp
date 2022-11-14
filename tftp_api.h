// Modified by Kollins G. Lima <kgl2@cin.ufpe.br>
#ifndef TFTP_TFTP_API_H
#define TFTP_TFTP_API_H

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>

/**
 * @brief Struct to hold the tftp options.
 */
typedef struct TftpHandler* TftpHandlerPtr;

/**
 * @brief Enum with possible return from interface functions.
 * Possible return values are:
 * - TFTP_OK:                    Operation was successful.
 * - TFTP_ERROR:                 Generic error.
 */
typedef enum {
    TFTP_OK = 0,
    TFTP_ERROR
} TftpOperationResult;

/*
*******************************************************************************
                                   CALLBACKS
*******************************************************************************
*/

/**
 * @brief TFTP error callback. This callback is called when the client
 * receives an error from the server.
 *
 * @param[in]   error_code      Error code.
 * @param[in]   error_msg       Error message.
 * @param[in]   context         Context passed to the callback.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
typedef TftpOperationResult (*tftp_error_callback) (
        short error_code,
        const char *error_message,
        void *context
);

/**
 * @brief TFTP data received callback. This callback is called when the client
 * receives data from the server related to the current fetch operation.
 *
 * @param[in]   data_size       Size of the data received.
 * @param[in]   context         Context passed to the callback.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
typedef TftpOperationResult (*tftp_fetch_data_received_callback) (
        int data_size,
        void *context
);

/*
*******************************************************************************
                                   FUNCTIONS
*******************************************************************************
*/

/**
 * @brief Init TFTP options with default values.
 * The option struct must be freed with destroy_tftp_options()
 * when the application is done with it.
 *
 * @param[out] handler the pointer to the tftp handler.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
TftpOperationResult create_tftp_handler(
        TftpHandlerPtr *handler
        );

/**
 * @brief Destroy TFTP options struct.
 *
 * @param[in] handler the pointer to the tftp handler.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
TftpOperationResult destroy_tftp_handler(
        TftpHandlerPtr *handler
        );

/**
 * @brief Register TFTP error callback
 *
 * @param[in] handler the pointer to the tftpd handler.
 * @param[in] callback the callback to register.
 * @param[in] context the user context.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
TftpOperationResult register_tftp_error_callback(
        const TftpHandlerPtr handler,
        tftp_error_callback callback,
        void *context
);

/**
 * @brief Register TFTP fetch data received callback
 *
 * @param[in] handler the pointer to the tftpd handler.
 * @param[in] callback the callback to register.
 * @param[in] context the user context.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
TftpOperationResult register_tftp_fetch_data_received_callback(
        const TftpHandlerPtr handler,
        tftp_fetch_data_received_callback callback,
        void *context
);

/**
 * @brief Set host and port for TFTP connection
 *
 * @param[in] handler the pointer to the tftp handler.
 * @param[in] host the host to connect to.
 * @param[in] port the port to connect to.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
TftpOperationResult set_connection(
        TftpHandlerPtr handler,
        const char* host,
        const int port
        );

/**
 * @brief Configure TFTP. Call this function before
 * calling send_file() or fetch_file() to configure
 * connection parameters.
 *
 * @param[in] handler the pointer to the tftp handler.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
TftpOperationResult config_tftp(
        const TftpHandlerPtr handler
);

/**
 * @brief Send a file through TFTP.
 *
 * @param[in] handler the pointer to the tftp handler.
 * @param[in] filename the name of the file to send.
 * This is the name the server will save the file as.
 * @param[in] fp the file pointer to send.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
TftpOperationResult send_file(
        TftpHandlerPtr handler,
        const char* filename,
        FILE *fp
        );

/**
 * @brief Fetches a file through TFTP.
 *
 * @param[in] handler the pointer to the tftp handler.
 * @param[in] filename the name of the file to fetch.
 * @param[in] fp the file pointer to receive data.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
TftpOperationResult fetch_file(
        TftpHandlerPtr handler,
        const char* filename,
        FILE *fp
        );

#ifdef __cplusplus
}
#endif

#endif //TFTP_TFTP_API_H
