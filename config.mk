# version
VERSION = 0.1

# paths
DEST 	:= /opt/fls

INSTALL_PATH := $(DEST)

CC 			?=
CFLAGS 		:= -Wall -Werror -pthread
DBGFLAGS 	:= -g -ggdb
TESTFLAGS 	:= -fprofile-arcs -ftest-coverage --coverage -lgcov
LINKFLAGS 	:= -shared

COBJFLAGS 	:= $(CFLAGS) -c -fPIC
test: COBJFLAGS 	+= $(TESTFLAGS)
debug: COBJFLAGS 	+= $(DBGFLAGS)
