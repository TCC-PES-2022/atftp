# ==========================================
#   Unity Project - A Test Framework for C
#   Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
#   [Released under MIT License. Please refer to license.txt for details]
# ==========================================

#We try to detect the OS we are running on, and adjust commands as needed
ifeq ($(OS),Windows_NT)
  ifeq ($(shell uname -s),) # not in a bash-like shell
	CLEANUP = del /F /Q
	MKDIR = mkdir
  else # in a bash-like shell, like msys
	CLEANUP = rm -f
	MKDIR = mkdir -p
  endif
	TARGET_EXTENSION=.exe
else
	CLEANUP = rm -f
	MKDIR = mkdir -p
	TARGET_EXTENSION=.out
endif

C_COMPILER=gcc
ifeq ($(shell uname -s), Darwin)
C_COMPILER=clang
endif

UNITY_ROOT=./Unity

#CFLAGS=-std=c89
CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -Wpointer-arith
CFLAGS += -Wcast-align
CFLAGS += -Wwrite-strings
CFLAGS += -Wswitch-default
CFLAGS += -Wunreachable-code
CFLAGS += -Winit-self
CFLAGS += -Wmissing-field-initializers
CFLAGS += -Wno-unknown-pragmas
CFLAGS += -Wstrict-prototypes
CFLAGS += -Wundef
CFLAGS += -Wold-style-definition
CFLAGS += -g -ggdb
#CFLAGS += -Wno-misleading-indentation

TARGET1_BASE=tftp
TARGET1 = $(TARGET1_BASE)$(TARGET_EXTENSION)
TARGET2_BASE=tftpd
TARGET2 = $(TARGET2_BASE)$(TARGET_EXTENSION)
SRC_FILES1=$(UNITY_ROOT)/src/unity.c atftp/TestTftp.c atftp/test_runners/Tftp_Runner.c
SRC_FILES2=$(UNITY_ROOT)/src/unity.c atftp/TestTftpd.c atftp/test_runners/Tftpd_Runner.c
INC_DIRS=-Isrc -I$(UNITY_ROOT)/src -I..
SYMBOLS=
LIBS1= -L.. -ltftp -lpthread
LIBS2= -L.. -ltftpd -lpthread

all: clean default

default: $(SRC_FILES1) $(SRC_FILES2)
	$(C_COMPILER) $(CFLAGS) $(INC_DIRS) $(SYMBOLS) $(SRC_FILES1) -o $(TARGET1) $(LIBS1)
	$(C_COMPILER) $(CFLAGS) $(INC_DIRS) $(SYMBOLS) $(SRC_FILES2) -o $(TARGET2) $(LIBS2)

atftp/test_runners/Tftp_Runner.c: atftp/TestTftp.c
	ruby $(UNITY_ROOT)/auto/generate_test_runner.rb atftp/TestTftp.c  atftp/test_runners/Tftp_Runner.c

atftp/test_runners/Tftpd_Runner.c: atftp/TestTftpd.c
	ruby $(UNITY_ROOT)/auto/generate_test_runner.rb atftp/TestTftpd.c  atftp/test_runners/Tftpd_Runner.c

clean:
	$(CLEANUP) $(TARGET1) $(TARGET2) test_runners/* *.txt

ci: CFLAGS += -Werror
ci: default
