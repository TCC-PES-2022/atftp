#ifndef TFTP_TFTPD_API_H
#define TFTP_TFTPD_API_H

#ifdef __cplusplus
extern "C"{
#endif

#include <pthread.h>
#include <stdio.h>

/**
 * @brief Struct to hold the tftpd options.
 */
typedef struct TftpdHandler* TftpdHandlerPtr;

/**
 * @brief Struct to hold the section info.
 */
typedef struct TftpdSectionHandler* TftpdSectionHandlerPtr;

/**
 * @brief Identifier for the section.
 */
typedef pthread_t SectionId;

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

/**
 * @brief Enum with possible section status.
 * Possible return values are:
 * - TFTPD_SECTION_OK:                    Section has been successfully completed.
 * - TFTPD_SECTION_ERROR:                 Section has failed.
 * - TFTPD_SECTION_UNDEFINED:             Section status is undefined.
 */
typedef enum {
    TFTPD_SECTION_OK = 0,
    TFTPD_SECTION_ERROR,
    TFTPD_SECTION_UNDEFINED
} TftpdSectionStatus;

/*
*******************************************************************************
                                   CALLBACKS
*******************************************************************************
*/

/**
 * @brief Callback for new section. This callback is called when a
 * client requests an operation to be performed.
 *
 * @param[in] section_handler the section handler.
 * @param[in] context the user context.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
typedef TftpdOperationResult (*section_started) (
        const TftpdSectionHandlerPtr section_handler,
        void *context
        );

/**
* @brief Callback for end section. This callback is called when a
 * client operation is finished.
 *
 * @param[in] section_handler the section handler.
 * @param[in] context the user context.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
typedef TftpdOperationResult (*section_finished) (
        const TftpdSectionHandlerPtr section_handler,
        void *context
        );

/**
 * @brief Open file callback. This callback is called when
 * the server needs to open a file, so a file pointer must
 * be opened for the filename with the given mode.
 *
 * If you want to use the default open file function, don't
 * register this callback. The server will open the file
 * based on the file name received.
 *
 * If you open the file, you must close it. Use the
 * close_file_callback for that.
 *
 * @param[out] fd the file pointer to open.
 * @param[in] filename the name of the file to open.
 * @param[in] mode the mode to open the file in.
 * @param[in] context the user context.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
typedef TftpdOperationResult (*open_file_callback) (
        FILE **fd,
        char *filename,
        char* mode,
        void *context
        );

/**
 * @brief Close file callback. This callback is called when
 * the server needs to close a file. If you register
 * open_file_callback, register this callback too.
 *
 * @param[in] fd the file pointer to close.
 * @param[in] context the user context.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
typedef TftpdOperationResult (*close_file_callback) (
        FILE *fd,
        void *context
);

/*
*******************************************************************************
                                   FUNCTIONS
*******************************************************************************
*/

/**
 * @brief Init TFTPD handler with default values.
 * The option struct must be freed with destroy_transfer_handler()
 * when the application is done with it.
 *
 * @param[out] handler the pointer to the tftpd handler.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult create_tftpd_handler(
        TftpdHandlerPtr *handler
        );

/**
 * @brief Destroy TFTPD handler struct.
 *
 * @param[in] handler the pointer to the tftpd handler.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult destroy_tftpd_handler(
        TftpdHandlerPtr *handler
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
        const TftpdHandlerPtr handler,
        const int port
        );

/**
 * @brief Set timeout. This is the time the server will wait for a client
 * to send a request. If the client doesn't send a request within this time,
 * the server will close the connection. The default is 300 seconds.
 * If you want to disable the timeout, set the timeout to 0.
 *
 * @param[in] handler the pointer to the tftpd handler.
 * @param[in] timeout the timeout in seconds.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult set_timeout(
        const TftpdHandlerPtr handler,
        const int timeout
);

/**
 * @brief Register open file callback.
 *
 * @param[in] handler the pointer to the tftpd handler.
 * @param[in] callback the callback to register.
 * @param[in] context the user context.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult register_open_file_callback(
        const TftpdHandlerPtr handler,
        open_file_callback callback,
        void *context
        );

/**
 * @brief Register close file callback.
 *
 * @param[in] handler the pointer to the tftpd handler.
 * @param[in] callback the callback to register.
 * @param[in] context the user context.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult register_close_file_callback(
        const TftpdHandlerPtr handler,
        close_file_callback callback,
        void *context
);

/**
 * @brief Register section start callback.
 *
 * @param[in] handler the pointer to the tftpd handler.
 * @param[in] callback the callback to register.
 * @param[in] context the user context.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult register_section_started_callback(
        const TftpdHandlerPtr handler,
        section_started callback,
        void *context
);

/**
 * @brief Register section finished callback.
 *
 * @param[in] handler the pointer to the tftpd handler.
 * @param[in] callback the callback to register.
 * @param[in] context the user context.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult register_section_finished_callback(
        const TftpdHandlerPtr handler,
        section_finished callback,
        void *context
);

/**
 * @brief Get identifier for the section.
 *
 * @param[in] section_handler the pointer to the section handler.
 * @param[out] section_id the port to use for TFTPD communication.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult get_section_id(
        const TftpdSectionHandlerPtr section_handler,
        SectionId *id
);

/**
 * @brief Get section status. Call this function from the section_finished
 * callback to check if the section was successful.
 *
 * @param[in] section_handler the pointer to the section handler.
 * @param[out] status the status of the section.
 *
 * @return TFTPD_OK if success.
 * @return TFTPD_ERROR otherwise.
 */
TftpdOperationResult get_section_status(
        const TftpdSectionHandlerPtr section_handler,
        TftpdSectionStatus *status
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
TftpdOperationResult start_listening(
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
TftpdOperationResult stop_listening(
        const TftpdHandlerPtr handler
        );

#ifdef __cplusplus
}
#endif

#endif //TFTP_TFTPD_API_H
