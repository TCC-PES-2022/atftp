#ifndef TFTP_TFTP_API_H
#define TFTP_TFTP_API_H

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
TftpOperationResult cretate_tftp_handler(
        TftpHandlerPtr handler
        );

/**
 * @brief Destroy TFTP options struct.
 *
 * @param[in] handler the pointer to the tftp handler.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
TftpdOperationResult destroy_tftp_handler(
        TftpdHandlerPtr handler
        );

/**
 * @brief Init TFTP with default values.
 *
 * @param[in] handler the pointer to the tftp handler.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
TftpdOperationResult init_tftp(
        const TftpdHandlerPtr handler
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
TftpdOperationResult set_connection(
        TftpdHandlerPtr handler,
        const char* host,
        const int port
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
TftpdOperationResult send_file(
        TftpdHandlerPtr handler,
        char* filename,
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
TftpdOperationResult fetch_file(
        TftpdHandlerPtr handler,
        char* filename,
        FILE *fp
        );

#endif //TFTP_TFTP_API_H
