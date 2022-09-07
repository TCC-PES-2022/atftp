include config.mk

# path macros
BIN_PATH := bin
SRC_PATH := atftp
REPORT_PATH := report

CLIENT_DEPS := libtftp.so
SERVER_DEPS := libtftpd.so

# compile macros
TARGET_NAME_CLIENT := tftp_client
TARGET_CLIENT := $(BIN_PATH)/$(TARGET_NAME_CLIENT)

TARGET_NAME_SERVER := tftp_server
TARGET_SERVER := $(BIN_PATH)/$(TARGET_NAME_SERVER)

# src files & obj files
SRC_CLIENT := $(UNITY_ROOT)/src/unity.c $(SRC_PATH)/TestTftp.c $(SRC_PATH)/Tftp_Runner.c
OBJ_CLIENT := $(addsuffix .o, $(basename $(SRC_CLIENT)))

SRC_SERVER := $(UNITY_ROOT)/src/unity.c $(SRC_PATH)/TestTftpd.c $(SRC_PATH)/Tftpd_Runner.c
OBJ_SERVER := $(addsuffix .o, $(basename $(SRC_SERVER)))

LIBS_CLIENT := $(LDLIBS) -ltftp
LIBS_SERVER := $(LDLIBS) -ltftpd

# clean files list
DISTCLEAN_LIST := $(OBJ_CLIENT) \
				  $(OBJ_SERVER) \
				  *.so

CLEAN_LIST := $(TARGET_CLIENT) 	\
			  $(TARGET_SERVER) 	\
			  *.txt 			\
			  *.gcov			\
			  *.gcno			\
			  *.gcda			\
			  $(SRC_PATH)/*.gcov \
			  $(SRC_PATH)/*.gcno \
			  $(SRC_PATH)/*.gcda \
			  $(DISTCLEAN_LIST)

# default rule
default: all

# non-phony targets
$(TARGET_CLIENT): $(CLIENT_DEPS) $(OBJ_CLIENT)
	$(CC) $(LINKFLAGS) -o $@ $(OBJ_CLIENT) $(CFLAGS) $(INCFLAGS) $(LDFLAGS) $(LIBS_CLIENT)

$(TARGET_SERVER): $(CLIENT_DEPS) $(OBJ_SERVER)
	$(CC) $(LINKFLAGS) -o $@ $(OBJ_SERVER) $(CFLAGS) $(INCFLAGS) $(LDFLAGS) $(LIBS_SERVER)

$(CLIENT_DEPS):
	cd .. && $(MAKE) test

$(SERVER_DEPS):
	cd .. && $(MAKE) test

%.o: %.c*
	$(CC) $(COBJFLAGS) -o $@ $< $(INCFLAGS)

# TODO: for some reason, the server runner is being deleted after build.
# don't worry, build will still work, but it's annoying.
%_Runner.o: %_Runner.c
	$(CC) $(COBJFLAGS) -o $@ $< $(INCFLAGS)

%_Runner.c: Test%.c
	ruby $(UNITY_ROOT)/auto/generate_test_runner.rb $< $@

# phony rules
.PHONY: makedir
makedir:
	@mkdir -p $(BIN_PATH)

.PHONY: all
all: makedir $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: client
client: makedir $(TARGET_CLIENT)

.PHONY: server
server: makedir $(TARGET_SERVER)

.PHONY: runclient
runclient:
	LD_LIBRARY_PATH=. ./$(TARGET_CLIENT)

.PHONY: runserver
runserver:
	LD_LIBRARY_PATH=. ./$(TARGET_SERVER)

.PHONY: report
report:
	@mkdir -p $(REPORT_PATH)
	find .. -name "*.gcov" -exec gcov {} --object-directory ../obj \;
	lcov --capture --directory ../obj --output-file $(REPORT_PATH)/coverage.info
	genhtml $(REPORT_PATH)/coverage.info --output-directory $(REPORT_PATH)

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)

.PHONY: distclean
distclean:
	@echo CLEAN $(DISTCLEAN_LIST)
	@rm -f $(DISTCLEAN_LIST)