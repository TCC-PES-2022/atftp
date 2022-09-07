#include "unity.h"
#include "tftpd_api.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdio.h>

#define BUF_SIZE 100

TftpdHandlerPtr handler = NULL;
char test_string[BUF_SIZE] = "HELLO TFTP TEST";
int port = 6905;
int timeout = 5;
char server_dir[BUF_SIZE] = "/tmp/";

typedef struct {
    TftpdHandlerPtr handler;
    TftpdOperationResult result;
    SectionId section_id;
    TftpdSectionStatus section_status;
    sem_t section_sem;
    FILE *fp;
    int use_memory;
    char memory_buffer[BUF_SIZE];
} TestServer;

void setUp(void)
{
    create_tftpd_handler(&handler);
}

void tearDown(void)
{
    destroy_tftpd_handler(&handler);
    handler = NULL;
}

void *start_listening_thread(void *arg)
{
    TestServer *server = (TestServer *)arg;
    server->result = start_listening(server->handler);
    return NULL;
}

TftpdOperationResult section_started_cbk (
        const TftpdSectionHandlerPtr section_handler,
        void *context)
{
    SectionId id;
    get_section_id(section_handler, &id);
    fprintf(stdout, "SECTION STARTED FOR: %lu\n", id);
    if (context != NULL) {
        TestServer *server = (TestServer *)context;
        server->section_id = id;
    }
    return TFTPD_OK;
}

TftpdOperationResult section_finished_cbk (
        const TftpdSectionHandlerPtr section_handler,
        void *context)
{
    SectionId id;
    get_section_id(section_handler, &id);
    fprintf(stdout, "SECTION FINISHED FOR: %lu\n", id);

    TftpdSectionStatus status;
    get_section_status(section_handler, &status);

    if (context != NULL) {
        TestServer *server = (TestServer *)context;
        if (id == server->section_id) {
            server->section_status = status;
        }
        sem_post(&server->section_sem);
    }
    return TFTPD_OK;
}

TftpdOperationResult open_file_cbk (
        FILE **fd, char *filename, char* mode, void *context)
{
    fprintf(stdout, "OPEN FILE REQUEST: %s, %s\n", filename, mode);
    if (context != NULL) {
        TestServer *server = (TestServer *)context;
        if (server->use_memory) {
            *fd = fmemopen(((TestServer *)context)->memory_buffer, BUF_SIZE, mode);
        } else {
            *fd = fopen(filename, mode);
        }
        server->fp = *fd;
    } else {
        *fd = fopen(filename, mode);
    }
    return TFTPD_OK;
}

TftpdOperationResult close_file_cbk (
        FILE *fd,
        void *context)
{
    if (fd != NULL) {
        fclose(fd);
        return TFTPD_OK;
    }
    return TFTPD_ERROR;
}


void test_SetPort_ShouldReturnTFTPDOK(void)
{
    TftpdOperationResult result = set_port(handler, port);
    TEST_ASSERT_EQUAL(TFTPD_OK, result);
}

void test_SetTimeout_ShouldReturnTFTPDOK(void)
{
    TftpdOperationResult result = set_timeout(handler, timeout);
    TEST_ASSERT_EQUAL(TFTPD_OK, result);
}

void test_RegisterOpenFileCallback_ShouldReturnTFTPDOK(void)
{
    TftpdOperationResult result = register_open_file_callback(handler, open_file_cbk, NULL);
    TEST_ASSERT_EQUAL(TFTPD_OK, result);
}

void test_RegisterSectionStartedCallback_ShouldReturnTFTPDOK(void)
{
    TftpdOperationResult result = register_section_started_callback(handler, section_started_cbk, NULL);
    TEST_ASSERT_EQUAL(TFTPD_OK, result);
}

void test_RegisterSectionFinishedCallback_ShouldReturnTFTPDOK(void)
{
    TftpdOperationResult result = register_section_finished_callback(handler, section_finished_cbk, NULL);
    TEST_ASSERT_EQUAL(TFTPD_OK, result);
}

void test_RegisterCloseFileCallback_ShouldReturnTFTPDOK(void)
{
    TftpdOperationResult result = register_close_file_callback(handler, close_file_cbk, NULL);
    TEST_ASSERT_EQUAL(TFTPD_OK, result);
}

void test_StartListeningTimeout_ShouldReturnTFTPDOK(void)
{
    if (set_port(handler, port) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("Failed to set port");
    }
    if (set_timeout(handler, timeout) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("Failed to set timeout");
    }
    TftpdOperationResult result = start_listening(handler);
    TEST_ASSERT_EQUAL(TFTPD_OK, result);
}

void test_StartStopListening_ShouldReturnTFTPDOK(void)
{
    if (set_port(handler, port) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_port failed");
    }
    if (set_timeout(handler, timeout) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_timeout failed");
    }

    TestServer server;
    server.handler = handler;
    pthread_t thread;
    pthread_create(&thread, NULL, start_listening_thread, &server);
    stop_listening(handler);
    pthread_join(thread, NULL);
    TEST_ASSERT_EQUAL(TFTPD_OK, server.result);
}

void test_ReceiveFileToDisk_ShouldReturnTFTPDOK(void)
{
    // Create test files
    char test_file1[BUF_SIZE] = {"test_server_receive_to_disk1.txt"};
    char test_file2[BUF_SIZE] = {"test_server_receive_to_disk2.txt"};
    FILE *fp = fopen(test_file1, "w");
    if (fp == NULL) {
        TEST_FAIL_MESSAGE("fopen failed");
    }
    fprintf(fp, "%s", test_string);
    fclose(fp);

    // Wait for tester to setup the client for test
    fprintf(stdout, "Prepare client to send file %s on port %d\n", test_file1, port);
    fprintf(stdout, "Send with remote name %s\n", test_file2);
    fprintf(stdout, "[Enter to continue]");
    getchar();

    // Start server configuration
    if (set_port(handler, port) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_port failed");
    }
    if (set_timeout(handler, timeout) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_timeout failed");
    }

    TestServer server_test;
    if(register_section_started_callback(handler, section_started_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_section_started_callback failed");
    }
    if (register_section_finished_callback(handler, section_finished_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_section_finished_callback failed");
    }
    if (register_open_file_callback(handler, open_file_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_open_file_callback failed");
    }
    if (register_close_file_callback(handler, close_file_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_close_file_callback failed");
    }
    server_test.handler = handler;
    server_test.section_status = TFTPD_SECTION_UNDEFINED;
    server_test.use_memory = 0;
    sem_init(&server_test.section_sem, 0, 0);

    // Start server
    pthread_t thread_server;
    pthread_create(&thread_server, NULL, start_listening_thread, &server_test);

    // Wait for server to receive file
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        TEST_FAIL_MESSAGE("clock_gettime failed");
    }
    ts.tv_sec += timeout*2;
    sem_timedwait(&server_test.section_sem, &ts);
    sem_destroy(&server_test.section_sem);

    // Stop server
    stop_listening(handler);
    pthread_join(thread_server, NULL);

    // Check if file was correctly received
    fp = fopen(test_file2, "r");
    if (fp == NULL) {
        TEST_FAIL_MESSAGE("fopen failed");
    }
    char line[BUF_SIZE];
    fgets(line, BUF_SIZE, fp);
    fclose(fp);

    TEST_ASSERT_EQUAL(TFTPD_OK, server_test.result);
    TEST_ASSERT_EQUAL(TFTPD_SECTION_OK, server_test.section_status);
    TEST_ASSERT_EQUAL_STRING(test_string, line);
}

void test_ReceiveFileToMemory_ShouldReturnTFTPDOK(void)
{
    // Create test files
    char test_file1[BUF_SIZE] = {"test_server_receive_to_mem1.txt"};
    FILE *fp = fopen(test_file1, "w");
    if (fp == NULL) {
        TEST_FAIL_MESSAGE("fopen failed");
    }
    fprintf(fp, "%s", test_string);
    fclose(fp);

    // Wait for tester to setup the client for test
    fprintf(stdout, "Prepare client to send file %s on port %d\n", test_file1, port);
    fprintf(stdout, "[Enter to continue]");
    getchar();

    // Start server configuration
    if (set_port(handler, port) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_port failed");
    }
    if (set_timeout(handler, timeout) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_timeout failed");
    }

    TestServer server_test;
    if(register_section_started_callback(handler, section_started_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_section_started_callback failed");
    }
    if (register_section_finished_callback(handler, section_finished_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_section_finished_callback failed");
    }
    if (register_open_file_callback(handler, open_file_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_open_file_callback failed");
    }
    if (register_close_file_callback(handler, close_file_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_close_file_callback failed");
    }
    server_test.handler = handler;
    server_test.section_status = TFTPD_SECTION_UNDEFINED;
    server_test.use_memory = 1;
    sem_init(&server_test.section_sem, 0, 0);

    // Start server
    pthread_t thread_server;
    pthread_create(&thread_server, NULL, start_listening_thread, &server_test);

    // Wait for server to receive file
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        TEST_FAIL_MESSAGE("clock_gettime failed");
    }
    ts.tv_sec += timeout*2;
    sem_timedwait(&server_test.section_sem, &ts);
    sem_destroy(&server_test.section_sem);

    // Stop server
    stop_listening(handler);
    pthread_join(thread_server, NULL);

    TEST_ASSERT_EQUAL(TFTPD_OK, server_test.result);
    TEST_ASSERT_EQUAL(TFTPD_SECTION_OK, server_test.section_status);
}

void test_SendFileFromDisk_ShouldReturnTFTPDOK(void)
{
    // Create test files
    char test_file1[BUF_SIZE] = {"test_server_send_from_disk1.txt"};
    char test_file2[BUF_SIZE] = {"test_server_send_from_disk2.txt"};
    FILE *fp = fopen(test_file1, "w");
    if (fp == NULL) {
        TEST_FAIL_MESSAGE("fopen failed");
    }
    fprintf(fp, "%s", test_string);
    fclose(fp);

    // Wait for tester to setup the client for test
    fprintf(stdout, "Prepare client to receive file %s on port %d\n", test_file1, port);
    fprintf(stdout, "Save it with local name %s\n", test_file2);
    fprintf(stdout, "[Enter to continue]");
    getchar();

    // Start server configuration
    if (set_port(handler, port) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_port failed");
    }
    if (set_timeout(handler, timeout) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_timeout failed");
    }

    TestServer server_test;
    if(register_section_started_callback(handler, section_started_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_section_started_callback failed");
    }
    if (register_section_finished_callback(handler, section_finished_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_section_finished_callback failed");
    }
    if (register_open_file_callback(handler, open_file_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_open_file_callback failed");
    }
    if (register_close_file_callback(handler, close_file_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_close_file_callback failed");
    }
    server_test.handler = handler;
    server_test.section_status = TFTPD_SECTION_UNDEFINED;
    server_test.use_memory = 0;
    sem_init(&server_test.section_sem, 0, 0);

    // Start server
    pthread_t thread_server;
    pthread_create(&thread_server, NULL, start_listening_thread, &server_test);

    // Wait for server to receive file
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        TEST_FAIL_MESSAGE("clock_gettime failed");
    }
    ts.tv_sec += timeout*2;
    sem_timedwait(&server_test.section_sem, &ts);
    sem_destroy(&server_test.section_sem);

    // Stop server
    stop_listening(handler);
    pthread_join(thread_server, NULL);

    // Check if file was correctly received
    fp = fopen(test_file2, "r");
    if (fp == NULL) {
        TEST_FAIL_MESSAGE("fopen failed");
    }
    char line[BUF_SIZE];
    fgets(line, BUF_SIZE, fp);
    fclose(fp);

    TEST_ASSERT_EQUAL(TFTPD_OK, server_test.result);
    TEST_ASSERT_EQUAL(TFTPD_SECTION_OK, server_test.section_status);
    TEST_ASSERT_EQUAL_STRING(test_string, line);
}

//    TEST_ASSERT_EQUAL_STRING(test_string, server_test.memory_buffer);

void test_SendFileFromMemory_ShouldReturnTFTPDOK(void)
{
    // Create test files
    char test_file1[BUF_SIZE] = {"test_server_send_from_mem1.txt"};
    TestServer server_test;
    memset(server_test.memory_buffer, 0, BUF_SIZE);
    FILE *fp = fmemopen(server_test.memory_buffer, BUF_SIZE, "w");
    if (fp == NULL) {
        TEST_FAIL_MESSAGE("fmemopen failed");
    }
    fprintf(fp, "%s", test_string);
    fclose(fp);

    // Wait for tester to setup the client for test
    fprintf(stdout, "Prepare client to receive file %s on port %d\n", test_file1, port);
    fprintf(stdout, "[Enter to continue]");
    getchar();

    // Start server configuration
    if (set_port(handler, port) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_port failed");
    }
    if (set_timeout(handler, timeout) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_timeout failed");
    }

    if(register_section_started_callback(handler, section_started_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_section_started_callback failed");
    }
    if (register_section_finished_callback(handler, section_finished_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_section_finished_callback failed");
    }
    if (register_open_file_callback(handler, open_file_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_open_file_callback failed");
    }
    if (register_close_file_callback(handler, close_file_cbk, &server_test) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("register_close_file_callback failed");
    }
    server_test.handler = handler;
    server_test.section_status = TFTPD_SECTION_UNDEFINED;
    server_test.use_memory = 1;
    sem_init(&server_test.section_sem, 0, 0);

    // Start server
    pthread_t thread_server;
    pthread_create(&thread_server, NULL, start_listening_thread, &server_test);

    // Wait for server to receive file
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        TEST_FAIL_MESSAGE("clock_gettime failed");
    }
    ts.tv_sec += timeout*2;
    sem_timedwait(&server_test.section_sem, &ts);
    sem_destroy(&server_test.section_sem);

    // Stop server
    stop_listening(handler);
    pthread_join(thread_server, NULL);

    // Check if file was correctly received
    fp = fopen(test_file1, "r");
    if (fp == NULL) {
        TEST_FAIL_MESSAGE("fopen failed");
    }
    char line[BUF_SIZE];
    fgets(line, BUF_SIZE, fp);
    fclose(fp);

    TEST_ASSERT_EQUAL(TFTPD_OK, server_test.result);
    TEST_ASSERT_EQUAL(TFTPD_SECTION_OK, server_test.section_status);
    TEST_ASSERT_EQUAL_STRING(test_string, line);
}