#include "unity.h"
#include "tftp_api.h"
#include "tftp_def.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#define NUM_TESTS 6
#define BUF_SIZE 1024

TftpHandlerPtr handler = NULL;
char test_string[BUF_SIZE] = "HELLO TFTP TEST";
char host[BUF_SIZE] = "127.0.0.1";
int port = 60907;
char server_dir[BUF_SIZE] = "/tmp/";
char server_port[BUF_SIZE] = "60907";


int executed_tests = 0;                     //TODO: Mutex-me for thread safety
pid_t pid_server;

void start_tftp_server()
{
    pid_server = fork();
    if (pid_server == 0) {
        // Child process
        char *args[] = {"./atftp/bin/atftpd",
                        "--verbose=7",
                        "--trace",
                        "--port",
                        server_port,
                        "--daemon",
                        "--no-fork",
                        "--logfile",
                        "-",
                        server_dir,
                        NULL};
        execvp(args[0], args);
        fprintf(stdout, "*** ERROR STARTING TFTP SERVER ***\n");
        exit(1);
    }
}

void setUp(void)
{
    if (!executed_tests) {
        start_tftp_server();
    }
    create_tftp_handler(&handler);
}

void tearDown(void)
{
    destroy_tftp_handler(&handler);
    handler = NULL;
    executed_tests++;
    if (executed_tests == NUM_TESTS)
    {
        kill(pid_server, SIGTERM);
        waitpid(pid_server, NULL, 0);
    }
}

void test_SetConnection_ShouldReturnTFTPOK(void)
{
    TftpOperationResult result = set_connection(handler, host, port);
    TEST_ASSERT_EQUAL(TFTP_OK, result);
}

void test_ConfigTftp_ShouldReturnTFTPOK(void)
{
    set_connection(handler, host, port);
    TftpOperationResult result = config_tftp(handler);
    TEST_ASSERT_EQUAL(TFTP_OK, result);
}

void test_PutFile_FromDisk_ShouldReturnTFTPOKIfFileWasSent(void)
{
  TftpOperationResult result = TFTP_ERROR;

  set_connection(handler, host, port);
  config_tftp(handler);

  //Create file to send
  char test_file[BUF_SIZE] = "test_put_fromdisk.txt";
  FILE *fp = fopen(test_file, "w");
  if (!fp) {
      TEST_FAIL_MESSAGE("Failed to create test file");
  }
  fprintf(fp, "%s", test_string);
  fclose(fp);


  fp = fopen(test_file, "r");
  if (fp) {
      result = send_file(handler, test_file, fp);
      fclose(fp);
  }

  //Check if file was correctly sent
  char server_path[BUF_SIZE];
  strcpy(server_path, server_dir);
  strcat(server_path, test_file);

  char line[BUF_SIZE] = {0};
  fp = fopen(server_path, "r");
  if (fp) {
      fgets(line, BUF_SIZE, fp);
      fclose(fp);
  }

  TEST_ASSERT_EQUAL(TFTP_OK, result);
  TEST_ASSERT_EQUAL_STRING(test_string, line);
}

void test_PutFile_FromMemory_ShouldReturnTFTPOKIfFileWasSent(void)
{
    TftpOperationResult result = TFTP_ERROR;

    set_connection(handler, host, port);
    config_tftp(handler);

    //Create file to send
    char test_file[BUF_SIZE] = "test_put_frommemory.txt";
    FILE *fp = fmemopen(test_string, BUF_SIZE, "r");
    if (fp) {
        result = send_file(handler, test_file, fp);
        fclose(fp);
    }

    //Check if file was correctly sent
    char server_path[BUF_SIZE];
    strcpy(server_path, server_dir);
    strcat(server_path, test_file);

    char line[BUF_SIZE];
    fp = fopen(server_path, "r");
    if (fp) {
        fgets(line, BUF_SIZE, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL(TFTP_OK, result);
    TEST_ASSERT_EQUAL_STRING(test_string, line);
}

void test_GetFile_ToDisk_ShouldReturnTFTPOKIfFileWasSent(void)
{
    TftpOperationResult result = TFTP_ERROR;

    set_connection(handler, host, port);
    config_tftp(handler);

    //Create file to send
    char test_file1[BUF_SIZE] = "test_get_todisk_1.txt";
    char test_file2[BUF_SIZE] = "test_get_todisk_2.txt";
    FILE *fp = fopen(test_file1, "w");
    if (!fp) {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", test_string);
    fclose(fp);

    fp = fopen(test_file1, "r");
    if (fp) {
        //Save in server with another name so we can get it back
        //without overwriting the original file
        send_file(handler, test_file2, fp);
        fclose(fp);
    }

    //Get the file from server
    fp = fopen(test_file2, "w");
    if (fp) {
        result = fetch_file(handler, test_file2, fp);
        fclose(fp);
    }

    //Compare first file with second file
    char line[BUF_SIZE];
    fp = fopen(test_file2, "r");
    if (fp) {
        fgets(line, BUF_SIZE, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL(TFTP_OK, result);
    TEST_ASSERT_EQUAL_STRING(test_string, line);
}

void test_GetFile_ToMemory_ShouldReturnTFTPOKIfFileWasSent(void)
{
    TftpOperationResult result = TFTP_ERROR;

    set_connection(handler, host, port);
    config_tftp(handler);

    //Create file to send
    char test_file[BUF_SIZE] = "test_get_tomemory.txt";
    FILE *fp = fopen(test_file, "w");
    if (!fp) {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", test_string);
    fclose(fp);

    fp = fopen(test_file, "r");
    if (fp) {
        send_file(handler, test_file, fp);
        fclose(fp);
    }

    //Get the file from server
    char buffer[BUF_SIZE];
    fp = fmemopen(buffer, BUF_SIZE, "w");
    if (fp) {
        result = fetch_file(handler, test_file, fp);
        fclose(fp);
    }

    TEST_ASSERT_EQUAL(TFTP_OK, result);
    TEST_ASSERT_EQUAL_STRING(test_string, buffer);
}

void test_ConnectionTimeoutSend_ShouldReturnTFTPError(void)
{
    TftpOperationResult result = TFTP_OK;

    set_connection(handler, host, 59);
    config_tftp(handler);

    //Create file to send
    char test_file[BUF_SIZE] = "test_timeout_send.txt";
    FILE *fp = fopen(test_file, "w");
    if (!fp) {
        TEST_FAIL_MESSAGE("Failed to create test file");
    }
    fprintf(fp, "%s", test_string);
    fclose(fp);


    fp = fopen(test_file, "r");
    int tftp_operation_time = 0;
    if (fp) {
        time_t start = time(NULL);
        result = send_file(handler, test_file, fp);
        time_t end = time(NULL);
        tftp_operation_time = difftime(end, start);
        fclose(fp);
    }

    TEST_ASSERT_INT_WITHIN(1, TIMEOUT*(NB_OF_RETRY+1), tftp_operation_time);
    TEST_ASSERT_EQUAL(TFTP_ERROR, result);
}

void test_ConnectionTimeoutFetch_ShouldReturnTFTPError(void)
{
    TftpOperationResult result = TFTP_OK;

    set_connection(handler, host, 59);
    config_tftp(handler);


    char test_file[BUF_SIZE] = "test_timeout_fetch.txt";
    FILE *fp = fopen(test_file, "w");
    int tftp_operation_time = 0;
    if (fp) {
        time_t start = time(NULL);
        result = fetch_file(handler, test_file, fp);
        time_t end = time(NULL);
        tftp_operation_time = difftime(end, start);
        fclose(fp);
    }

    TEST_ASSERT_INT_WITHIN(1, TIMEOUT*(NB_OF_RETRY+1), tftp_operation_time);
    TEST_ASSERT_EQUAL(TFTP_ERROR, result);
}