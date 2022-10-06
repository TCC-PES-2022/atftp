# version
VERSION = 0.1

# paths
DESTDIR 	?= /tmp
DEP_PATH 	?= $(DESTDIR)

CC 			?= gcc
AR 			?= ar
CFLAGS 		:= -Wall -Werror -pthread
ARFLAGS		:= -rcv
DBGFLAGS 	:= -g -ggdb
TESTFLAGS 	:= -fprofile-arcs -ftest-coverage --coverage
#LINKFLAGS 	:= -shared

COBJFLAGS 	:= $(CFLAGS) -c -fPIC
test: COBJFLAGS 	+= $(TESTFLAGS)
test: LINKFLAGS 	+= -fprofile-arcs -lgcov
debug: COBJFLAGS 	+= $(DBGFLAGS)
