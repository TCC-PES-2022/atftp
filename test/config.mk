# version
VERSION = 0.1

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
COBJFLAGS 	:= $(CFLAGS) -c
LDFLAGS  	:= -L.
LDLIBS   	:= -lpthread
INCFLAGS   	:= -I$(UNITY_ROOT)/src -I..
