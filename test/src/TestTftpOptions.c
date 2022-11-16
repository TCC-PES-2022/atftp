#include "unity.h"
#include "tftp_api.h"
#include "tftpd_api.h"
#include "tftp_def.h"
#include <string.h>

#define DEFAULT_TFTP_PORT 59595
#define LOCALHOST "127.0.0.1"

#define BUF_SIZE 1024
#define TEST_STRING "test_string"
#define FILE_NAME1 "test_file1.txt"
#define FILE_NAME2 "test_file2.txt"

#define BLOCK_SIZE_DEFAULT_OPTION "512"
#define BLOCK_SIZE_OPTION "666"
#define PORT_OPTION "777"

TftpHandlerPtr client_handler = NULL;
TftpdHandlerPtr server_handler = NULL;
pthread_t thread_server;

typedef struct
{
    char key[BUF_SIZE];
    char value[BUF_SIZE];
} TftpOptionContext;

void *start_listening_thread(void *arg)
{
    TftpdHandlerPtr *handler = (TftpdHandlerPtr *)arg;
    start_listening(*handler);
    return NULL;
}

void setUp(void)
{
    create_tftp_handler(&client_handler);
    create_tftpd_handler(&server_handler);

    set_port(server_handler, DEFAULT_TFTP_PORT);
    pthread_create(&thread_server, NULL, start_listening_thread, &server_handler);
}

void tearDown(void)
{
    stop_listening(server_handler);
    pthread_join(thread_server, NULL);

    destroy_tftp_handler(&client_handler);
    client_handler = NULL;
    destroy_tftpd_handler(&server_handler);
    server_handler = NULL;
}

void test_SetBlockSizeOption_ShouldReturnTFTPOK(void)
{
    TEST_ASSERT_EQUAL(TFTP_OK, set_tftp_option(client_handler, TFTP_BLOCKSIZE_OPTION, BLOCK_SIZE_OPTION));
}

void test_SetPortOption_ShouldReturnTFTPOK(void)
{
    TEST_ASSERT_EQUAL(TFTP_OK, set_tftp_option(client_handler, TFTP_PORT_OPTION, PORT_OPTION));
}

TftpdOperationResult receivedOptionCkb(
    const TftpdSectionHandlerPtr section_handler,
    char *option,
    char *value,
    void *context)
{
    if (context != NULL)
    {
        printf("Received option %s with value %s\n", option, value);
        TftpOptionContext *ctx = (TftpOptionContext *)context;
        if (strcmp(option, ctx->key) == 0)
        {
            strcpy(ctx->value, value);
        }
    }
    return TFTPD_OK;
}

void test_SendBlockSizeOptionWRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "blksize");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_BLOCKSIZE_OPTION, BLOCK_SIZE_OPTION);
    config_tftp(client_handler);

    register_option_received_callback(server_handler, receivedOptionCkb, &context);

    // Create file to send
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME1, "r");
    if (fp)
    {
        send_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING(BLOCK_SIZE_OPTION, context.value);
}

void test_SendPortOptionWRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "port");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_PORT_OPTION, PORT_OPTION);
    config_tftp(client_handler);

    register_option_received_callback(server_handler, receivedOptionCkb, &context);

    // Create file to send
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME1, "r");
    if (fp)
    {
        send_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING(PORT_OPTION, context.value);
}

void test_SendBlockSizeOptionRRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "blksize");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_BLOCKSIZE_OPTION, BLOCK_SIZE_OPTION);
    config_tftp(client_handler);

    register_option_received_callback(server_handler, receivedOptionCkb, &context);

    // Create file to fetch
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME2, "w");
    if (fp)
    {
        fetch_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING(BLOCK_SIZE_OPTION, context.value);
}

void test_SendPortOptionRRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "port");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_PORT_OPTION, PORT_OPTION);
    config_tftp(client_handler);

    register_option_received_callback(server_handler, receivedOptionCkb, &context);

    // Create file to send
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME2, "w");
    if (fp)
    {
        fetch_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING(PORT_OPTION, context.value);
}

TftpOperationResult acceptedOptionCkb(
    char *option,
    char *value,
    void *context)
{
    if (context != NULL)
    {
        printf("Accepted option %s with value %s\n", option, value);
        TftpOptionContext *ctx = (TftpOptionContext *)context;
        if (strcmp(option, ctx->key) == 0)
        {
            strcpy(ctx->value, value);
        }
    }
    return TFTP_OK;
}

void test_CheckAcceptedBlockSizeOptionWRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "blksize");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_BLOCKSIZE_OPTION, BLOCK_SIZE_OPTION);
    config_tftp(client_handler);

    register_tftp_option_accepted_callback(client_handler, acceptedOptionCkb, &context);

    // Create file to send
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME1, "r");
    if (fp)
    {
        send_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING(BLOCK_SIZE_OPTION, context.value);
}

void test_CheckAcceptedPortOptionWRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "port");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_PORT_OPTION, PORT_OPTION);
    config_tftp(client_handler);

    register_tftp_option_accepted_callback(client_handler, acceptedOptionCkb, &context);

    // Create file to send
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME1, "r");
    if (fp)
    {
        send_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING(PORT_OPTION, context.value);
}

void test_CheckAcceptedBlockSizeOptionRRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "blksize");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_BLOCKSIZE_OPTION, BLOCK_SIZE_OPTION);
    config_tftp(client_handler);

    register_tftp_option_accepted_callback(client_handler, acceptedOptionCkb, &context);

    // Create file to send
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME2, "w");
    if (fp)
    {
        fetch_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING(BLOCK_SIZE_OPTION, context.value);
}

void test_CheckAcceptedPortOptionRRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "port");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_PORT_OPTION, PORT_OPTION);
    config_tftp(client_handler);

    register_tftp_option_accepted_callback(client_handler, acceptedOptionCkb, &context);

    // Create file to send
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME2, "w");
    if (fp)
    {
        fetch_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING(PORT_OPTION, context.value);
}

TftpdOperationResult rejectOptionCbk(
    const TftpdSectionHandlerPtr section_handler,
    char *option,
    char *value,
    void *context)
{
    return TFTPD_ERROR;
}

void test_RejectBlockSizeOptionWRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "blksize");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_BLOCKSIZE_OPTION, BLOCK_SIZE_OPTION);
    config_tftp(client_handler);

    register_option_received_callback(server_handler, rejectOptionCbk, NULL);
    register_tftp_option_accepted_callback(client_handler, acceptedOptionCkb, &context);

    // Create file to send
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME1, "r");
    if (fp)
    {
        send_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING("", context.value);
}

void test_RejectPortOptionWRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "port");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_PORT_OPTION, PORT_OPTION);
    config_tftp(client_handler);

    register_option_received_callback(server_handler, rejectOptionCbk, NULL);
    register_tftp_option_accepted_callback(client_handler, acceptedOptionCkb, &context);

    // Create file to send
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME1, "r");
    if (fp)
    {
        send_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING("", context.value);
}

void test_RejectBlockSizeOptionRRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "blksize");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_BLOCKSIZE_OPTION, BLOCK_SIZE_OPTION);
    config_tftp(client_handler);

    register_option_received_callback(server_handler, rejectOptionCbk, NULL);
    register_tftp_option_accepted_callback(client_handler, acceptedOptionCkb, &context);

    // Create file to fetch
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME2, "w");
    if (fp)
    {
        fetch_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING("", context.value);
}

void test_RejectPortOptionRRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "port");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_PORT_OPTION, PORT_OPTION);
    config_tftp(client_handler);

    register_option_received_callback(server_handler, rejectOptionCbk, NULL);
    register_tftp_option_accepted_callback(client_handler, acceptedOptionCkb, &context);

    // Create file to send
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME2, "w");
    if (fp)
    {
        fetch_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING("", context.value);
}

TftpdOperationResult changeOptionCkb(
    const TftpdSectionHandlerPtr section_handler,
    char *option,
    char *value,
    void *context)
{
    if (strcmp(option, "blksize") == 0)
    {
        strncpy(value, BLOCK_SIZE_DEFAULT_OPTION, VAL_SIZE);
    }
    return TFTPD_OK;
}

void test_ChangeBlockSizeOptionWRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "blksize");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_BLOCKSIZE_OPTION, BLOCK_SIZE_OPTION);
    config_tftp(client_handler);

    register_option_received_callback(server_handler, changeOptionCkb, NULL);
    register_tftp_option_accepted_callback(client_handler, acceptedOptionCkb, &context);

    // Create file to send
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME1, "r");
    if (fp)
    {
        send_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING(BLOCK_SIZE_DEFAULT_OPTION, context.value);
}

void test_ChangeBlockSizeOptionRRQ_ShouldReturnTFTPOK(void)
{
    TftpOptionContext context;
    strcpy(context.key, "blksize");
    context.value[0] = '\0';

    set_connection(client_handler, LOCALHOST, DEFAULT_TFTP_PORT);
    set_tftp_option(client_handler, TFTP_BLOCKSIZE_OPTION, BLOCK_SIZE_OPTION);
    config_tftp(client_handler);

    register_option_received_callback(server_handler, changeOptionCkb, NULL);
    register_tftp_option_accepted_callback(client_handler, acceptedOptionCkb, &context);

    // Create file to fetch
    FILE *fp = fopen(FILE_NAME1, "w");
    if (!fp)
    {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", TEST_STRING);
    fclose(fp);

    fp = fopen(FILE_NAME2, "w");
    if (fp)
    {
        fetch_file(client_handler, FILE_NAME1, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL_STRING(BLOCK_SIZE_DEFAULT_OPTION, context.value);
}