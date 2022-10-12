# version
VERSION = 0.1

DESTDIR 	?= /tmp
DEP_PATH 	?= $(DESTDIR)

UNITY_ROOT=./Unity

CC 			?=
CFLAGS 		+= -Wall
CFLAGS 		+= -Wextra
CFLAGS 		+= -Wpointer-arith
CFLAGS 		+= -Wcast-align
CFLAGS 		+= -Wwrite-strings
CFLAGS 		+= -Wswitch-default
CFLAGS 		+= -Wunreachable-code
CFLAGS 		+= -Winit-self
CFLAGS 		+= -Wmissing-field-initializers
CFLAGS 		+= -Wno-unknown-pragmas
CFLAGS 		+= -Wstrict-prototypes
CFLAGS 		+= -Wundef
CFLAGS 		+= -Wold-style-definition
CFLAGS 		+= -Wno-unused-parameter
CFLAGS 		+= -fprofile-arcs -ftest-coverage --coverage
DBGFLAGS 	:= -g -ggdb
COBJFLAGS 	:= $(CFLAGS) -c
LDFLAGS  	+= -L$(DEP_PATH)/lib
LDLIBS   	:= -lpthread -lgcov -pthread -ltftp -ltftpd
INCFLAGS   	:= -I$(UNITY_ROOT)/src -I$(DEP_PATH)/include -I..

debug: COBJFLAGS 		+= $(DBGFLAGS)
debugdeps: DEP_RULE    	:= debug
testdeps: DEP_RULE    	:= test
