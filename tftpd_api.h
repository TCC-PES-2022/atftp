#ifndef TFTP_TFTPD_API_H
#define TFTP_TFTPD_API_H

/**
 * @brief Struct to hold the tftpd options.
 */
typedef struct TftpdHandler* TftpdHandlerPtr;

/**
 * @brief Enum with possible return from interface functions.
 * Possible return values are:
 * - TFTPD_OK:                    Operation was successful.
 * - TFTPD_ERROR:                 Generic error.
 */
typedef enum {
    TFTPD_OK = 0,
    TFTPD_ERROR
} TftpdOperationResult;

/*
*******************************************************************************
                                   CALLBACKS
*******************************************************************************
*/

/**
 * @brief Open file callback. This callback is called when
 * the server needs to open a file, so a file pointer must
 * be opened for the filename with the given mode.
 *
 * If the file pointer returned is NULL, file failed to open.
 *
 * @param[in] filename the name of the file to open.
 * @param[out] fd the file pointer to open.
 * @param[in] mode the mode to open the file in.
 *
 * @return TFTP_OK if success.
 * @return TFTP_ERROR otherwise.
 */
typedef TftpdOperationResult (*open_file_callback) (char *filename, FILE **fd, char* mode);

/*
*******************************************************************************
                                   FUNCTIONS
*******************************************************************************
*/

/**
 * @brief Init TFTPD handler with default values.
 * The option struct must be freed with destroy_tftpd_options()
 * when the application is done with it.
 *
 * @param[out] handler the pointer to the tftpd handler.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult cretate_tftpd_handler(
        TftpdHandlerPtr handler
        );

/**
 * @brief Destroy TFTPD options struct.
 *
 * @param[in] handler the pointer to the tftpd handler.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult destroy_tftpd_handler(
        TftpdHandlerPtr handler
        );

/**
 * @brief Set port to use for TFTPD communication.
 *
 * @param[in] handler the pointer to the tftpd handler.
 * @param[in] port the port to use for TFTPD communication.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult set_port(
        TftpdHandlerPtr handler,
        const int port
        );

/**
 * @brief Register open file callback.
 *
 * @param[in] handler the pointer to the tftpd handler.
 * @param[in] callback the callback to register.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult register_open_file_callback(
        TftpdHandlerPtr handler,
        open_file_callback callback
        );

/**
 * @brief Start the TFTPD. This is a blocking function.
 * In order to stop be able to stop the server, call this function
 * from another thread and then use the stop_listening() function.
 *
 * @param[in] handler the pointer to the tftpd handler.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult start_listen(
        const TftpdHandlerPtr handler
        );

/**
 * @brief Request the TFTPD to stop listening. The success
 * return of this function doesn't mean the server has stopped,
 * it just means that the server will stop listening when it is
 * ready. When the server stops listening, the start_listening()
 * function will return.
 *
 * @param[in] handler the pointer to the tftpd handler.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult stop_listen(
        const TftpdHandlerPtr handler
        );

#endif //TFTP_TFTPD_API_H
