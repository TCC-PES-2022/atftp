include config.mk

# path macros
BIN_PATH := bin
OBJ_PATH := obj
SRC_PATH := ./
DBG_PATH := debug
TEST_PATH := test

# compile macros
TARGET_NAME_CLIENT := libtftp.so
TARGET_CLIENT := $(BIN_PATH)/$(TARGET_NAME_CLIENT)
TARGET_CLIENT_DEBUG := $(DBG_PATH)/$(TARGET_NAME_CLIENT)
TARGET_CLIENT_TEST := $(TEST_PATH)/$(TARGET_NAME_CLIENT)

TARGET_NAME_SERVER := libtftpd.so
TARGET_SERVER := $(BIN_PATH)/$(TARGET_NAME_SERVER)
TARGET_SERVER_DEBUG := $(DBG_PATH)/$(TARGET_NAME_SERVER)
TARGET_SERVER_TEST := $(TEST_PATH)/$(TARGET_NAME_SERVER)

# src files & obj files
SRC_CLIENT := tftp.c tftp_io.c logger.c options.c tftp_def.c tftp_file.c tftp_mtftp.c
OBJ_CLIENT := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC_CLIENT)))))
OBJ_CLIENT_DEBUG := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC_CLIENT)))))
OBJ_CLIENT_TEST := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC_CLIENT)))))

SRC_SERVER := tftpd.c logger.c options.c stats.c tftp_io.c tftp_def.c tftpd_file.c tftpd_list.c \
			  tftpd_mcast.c tftpd_pcre.c tftpd_mtftp.c
OBJ_SERVER := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC_SERVER)))))
OBJ_SERVER_DEBUG := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC_SERVER)))))
OBJ_SERVER_TEST := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC_SERVER)))))

# clean files list
DISTCLEAN_LIST := $(OBJ_CLIENT) \
                  $(OBJ_CLIENT_DEBUG) \
                  $(OBJ_CLIENT_TEST) \
                  $(OBJ_SERVER) \
                  $(OBJ_SERVER_DEBUG) \
                  $(OBJ_SERVER_TEST)
CLEAN_LIST := $(TARGET_CLIENT) \
			  $(TARGET_CLIENT_DEBUG) \
			  $(TARGET_CLIENT_TEST) \
			  $(TARGET_SERVER) \
              $(TARGET_SERVER_DEBUG) \
              $(TARGET_SERVER_TEST) \
              $(DISTCLEAN_LIST)

# default rule
default: all

# non-phony targets
$(TARGET_CLIENT): $(OBJ_CLIENT)
	$(CC) $(LINKFLAGS) -o $@ $(OBJ_CLIENT) $(CFLAGS)

$(TARGET_SERVER): $(OBJ_SERVER)
	$(CC) $(LINKFLAGS) -o $@ $(OBJ_SERVER) $(CFLAGS)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c*
	$(CC) $(COBJFLAGS) -o $@ $<

$(TARGET_CLIENT_DEBUG): $(OBJ_CLIENT_DEBUG)
	$(CC) $(CFLAGS) $(DBGFLAGS) $(LINKFLAGS) $(OBJ_CLIENT_DEBUG) -o $@

$(TARGET_SERVER_DEBUG): $(OBJ_SERVER_DEBUG)
	$(CC) $(CFLAGS) $(DBGFLAGS) $(LINKFLAGS) $(OBJ_SERVER_DEBUG) -o $@

$(TARGET_CLIENT_TEST): $(OBJ_CLIENT_TEST)
	$(CC) $(CFLAGS) $(TESTFLAGS) $(LINKFLAGS) $(OBJ_CLIENT_TEST) -o $@

$(TARGET_SERVER_TEST): $(OBJ_SERVER_TEST)
	$(CC) $(CFLAGS) $(TESTFLAGS) $(LINKFLAGS) $(OBJ_SERVER_TEST) -o $@

# phony rules
.PHONY: makedir
makedir:
	@mkdir -p $(BIN_PATH) $(OBJ_PATH) $(DBG_PATH) $(TEST_PATH)

.PHONY: all
all: makedir $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: debug
debug: makedir $(TARGET_CLIENT_DEBUG) $(TARGET_SERVER_DEBUG)

.PHONY: test
test: makedir $(TARGET_CLIENT_TEST) $(TARGET_SERVER_TEST)

.PHONY: install
install: all
	mkdir -p $(INSTALL_PATH)/lib $(INSTALL_PATH)/include
	cp -f $(BIN_PATH)/*.so $(INSTALL_PATH)/lib
	cp -f *_api*.h $(INSTALL_PATH)/include

.PHONY: uninstall
uninstall:
	rm -rf $(INSTALL_PATH)/lib $(INSTALL_PATH)/include

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)

.PHONY: distclean
distclean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -rf $(BIN_PATH) $(OBJ_PATH) $(DBG_PATH)