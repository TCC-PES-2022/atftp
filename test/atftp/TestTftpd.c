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
#include <arpa/inet.h>
#include <stdlib.h>

#define BUF_SIZE 100
#define NUM_TESTS 12

#define PUT_FILE "-p"
#define GET_FILE "-g"

#define TIMEOUT 5
#define PORT    60905

int executed_tests = 0;                     //TODO: Mutex-me for thread safety
int pid_client_idx = 0;
pid_t pid_client[NUM_TESTS];

TftpdHandlerPtr handler = NULL;
char test_string[BUF_SIZE] = "HELLO TFTP TEST";
char server_dir[BUF_SIZE] = "/tmp/";
#define SOCKADDR_PRINT_ADDR_LEN INET6_ADDRSTRLEN

typedef struct {
    TftpdHandlerPtr handler;
    TftpdOperationResult result;
    SectionId section_id;
    char client_ip[SOCKADDR_PRINT_ADDR_LEN];
    TftpdSectionStatus section_status;
    sem_t section_sem;
    FILE *fp;
    int use_memory;
    char memory_buffer[BUF_SIZE];
} TestServer;

void start_tftp_client(char *operation,
                       char *local_file,
                       char *remote_file,
                       int port)
{
    pid_client[pid_client_idx] = fork();
    if (pid_client[pid_client_idx] == 0) {
        // Child process
        char port_str[BUF_SIZE];
        sprintf(port_str, "%d", port);
        char *args[] = {"./atftp/bin/atftp",
                        "--verbose",
                        "--trace",
                        operation,
                        "-l",
                        local_file,
                        "-r",
                        remote_file,
                        "127.0.0.1",
                        port_str,
                        NULL};
        execvp(args[0], args);
        fprintf(stdout, "*** ERROR STARTING TFTP CLIENT ***\n");
        exit(1);
    }
    pid_client_idx++;
}

void setUp(void)
{
    create_tftpd_handler(&handler);
}

void tearDown(void)
{
    destroy_tftpd_handler(&handler);
    handler = NULL;
    executed_tests++;
    if (executed_tests == NUM_TESTS)
    {
        for (int i = 0; i < pid_client_idx; i++) {
            kill(pid_client[i], SIGTERM);
            waitpid(pid_client[i], NULL, 0);
        }
    }
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
    char ip[SOCKADDR_PRINT_ADDR_LEN];
    get_section_id(section_handler, &id);
    get_client_ip(section_handler, ip);
    fprintf(stdout, "SECTION STARTED FOR: %lu - %s\n", id, ip);
    if (context != NULL) {
        TestServer *server = (TestServer *)context;
        server->section_id = id;
        strcpy(server->client_ip, ip);
    }
    return TFTPD_OK;
}

TftpdOperationResult section_finished_cbk (
        const TftpdSectionHandlerPtr section_handler,
        void *context)
{
    SectionId id;
    char ip[SOCKADDR_PRINT_ADDR_LEN];
    get_section_id(section_handler, &id);
    get_client_ip(section_handler, ip);
    fprintf(stdout, "SECTION FINISHED FOR: %lu - %s\n", id, ip);


    TftpdSectionStatus status;
    get_section_status(section_handler, &status);

    if (context != NULL) {
        TestServer *server = (TestServer *)context;
        if (id == server->section_id && strcmp(server->client_ip, ip) == 0) {
            server->section_status = status;
        }
        sem_post(&server->section_sem);
    }
    return TFTPD_OK;
}

TftpdOperationResult open_file_cbk (
        const TftpdSectionHandlerPtr section_handler,
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
        const TftpdSectionHandlerPtr section_handler,
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
    int test_port = PORT + executed_tests;
    TftpdOperationResult result = set_port(handler, test_port);
    TEST_ASSERT_EQUAL(TFTPD_OK, result);
}

void test_SetTimeout_ShouldReturnTFTPDOK(void)
{
    TftpdOperationResult result = set_server_timeout(handler, TIMEOUT);
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
    int test_port = PORT + executed_tests;
    if (set_port(handler, test_port) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("Failed to set port");
    }
    if (set_server_timeout(handler, TIMEOUT) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("Failed to set timeout");
    }
    TftpdOperationResult result = start_listening(handler);
    TEST_ASSERT_EQUAL(TFTPD_OK, result);
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

    // Start server configuration
    int test_port = PORT + executed_tests;
    if (set_port(handler, test_port) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_port failed");
    }
    if (set_server_timeout(handler, TIMEOUT) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_server_timeout failed");
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
    sleep(1);

    start_tftp_client(PUT_FILE, test_file1, test_file2, test_port);

    // Wait for server to receive file
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        TEST_FAIL_MESSAGE("clock_gettime failed");
    }
    ts.tv_sec += TIMEOUT*2;
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

    // Start server configuration
    int test_port = PORT + executed_tests;
    if (set_port(handler, test_port) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_port failed");
    }
    if (set_server_timeout(handler, TIMEOUT) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_server_timeout failed");
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
    sleep(1);

    start_tftp_client(PUT_FILE, test_file1, test_file1, test_port);

    // Wait for server to receive file
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        TEST_FAIL_MESSAGE("clock_gettime failed");
    }
    ts.tv_sec += TIMEOUT*2;
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

    // Start server configuration
    int test_port = PORT + executed_tests;
    if (set_port(handler, test_port) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_port failed");
    }
    if (set_server_timeout(handler, TIMEOUT) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_server_timeout failed");
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
    sleep(1);

    start_tftp_client(GET_FILE, test_file2, test_file1, test_port);

    // Wait for server to receive file
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        TEST_FAIL_MESSAGE("clock_gettime failed");
    }
    ts.tv_sec += TIMEOUT*2;
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

    // Start server configuration
    int test_port = PORT + executed_tests;
    if (set_port(handler, test_port) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_port failed");
    }
    if (set_server_timeout(handler, TIMEOUT) != TFTPD_OK) {
        TEST_FAIL_MESSAGE("set_server_timeout failed");
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
    sleep(1);

    start_tftp_client(GET_FILE, test_file1, test_file1, test_port);

    // Wait for server to receive file
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        TEST_FAIL_MESSAGE("clock_gettime failed");
    }
    ts.tv_sec += TIMEOUT*2;
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

void test_StartStopListening_ShouldReturnTFTPDOK(void)
{
    int test_port = PORT + executed_tests;
    if (set_port(handler, test_port) != TFTPD_OK)
    {
        TEST_FAIL_MESSAGE("set_port failed");
    }

    //Explicitly set the timeout to 0 to avoid exiting by timeout
    if (set_server_timeout(handler, 0) != TFTPD_OK)
    {
        TEST_FAIL_MESSAGE("set_server_timeout failed");
    }

    TestServer server;
    server.handler = handler;
    pthread_t thread;
    pthread_create(&thread, NULL, start_listening_thread, &server);
    sleep(TIMEOUT);
    stop_listening(handler);

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
    {
        TEST_FAIL_MESSAGE("clock_gettime() failed");
    }
    ts.tv_sec += TIMEOUT;
    if (pthread_timedjoin_np(thread, NULL, &ts) != 0)
    {
        TEST_FAIL_MESSAGE("pthread_timedjoin_np() failed");
    }
    TEST_ASSERT_EQUAL(TFTPD_OK, server.result);
}

}