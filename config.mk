# version
VERSION = 0.1

# paths
DESTDIR 	?= /tmp

CC 			?= gcc
AR 			?= ar
CFLAGS 		:= -Wall -Werror -pthread
ARFLAGS		 = rcs
DBGFLAGS 	:= -g -ggdb
TESTFLAGS 	:= -fprofile-arcs -ftest-coverage --coverage
#LINKFLAGS 	:= -shared

#COBJFLAGS 	:= $(CFLAGS) -c -fPIC
COBJFLAGS 	:= $(CFLAGS) -c
test: COBJFLAGS 	+= $(TESTFLAGS)
test: LINKFLAGS 	+= -fprofile-arcs -lgcov
debug: COBJFLAGS 	+= $(DBGFLAGS)
