# version
VERSION = 0.1

# paths
DEST 	:= /opt/fls
MODULE 	:= atftp

INSTALL_PATH := $(DEST)/$(MODULE)

CC 			?=
CFLAGS 		:= -Wall -Werror
DBGFLAGS 	:= -g -ggdb
TESTFLAGS 	:= -fprofile-arcs -ftest-coverage --coverage
LINKFLAGS 	:= -shared

COBJFLAGS 	:= $(CFLAGS) -c -fPIC
test: COBJFLAGS 	+= $(TESTFLAGS)
debug: COBJFLAGS 	+= $(DBGFLAGS)
