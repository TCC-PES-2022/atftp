include config.mk

# path macros
BIN_PATH := bin
OBJ_PATH := obj
SRC_PATH := ./
# DBG_PATH := debug

# compile macros
TARGET_NAME_CLIENT := libtftp.so
TARGET_CLIENT := $(BIN_PATH)/$(TARGET_NAME_CLIENT)
# TARGET_CLIENT_DEBUG := $(DBG_PATH)/$(TARGET_NAME_CLIENT)
# TARGET_CLIENT_TEST := $(BIN_PATH)/$(TARGET_NAME_CLIENT)

TARGET_NAME_SERVER := libtftpd.so
TARGET_SERVER := $(BIN_PATH)/$(TARGET_NAME_SERVER)
# TARGET_SERVER_DEBUG := $(DBG_PATH)/$(TARGET_NAME_SERVER)
# TARGET_SERVER_TEST := $(BIN_PATH)/$(TARGET_NAME_SERVER)

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
	$(CC) -o $@ $(LINKFLAGS) $(OBJ_CLIENT) $(CFLAGS)

$(TARGET_SERVER): $(OBJ_SERVER)
	$(CC) -o $@ $(LINKFLAGS) $(OBJ_SERVER) $(CFLAGS)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c*
	$(CC) $(COBJFLAGS) -o $@ $<

# $(TARGET_CLIENT_DEBUG): $(OBJ_CLIENT_DEBUG)
# 	$(CC) $(CFLAGS) $(DBGFLAGS) $(OBJ_CLIENT_DEBUG) -o $@ $(LINKFLAGS)

# $(TARGET_SERVER_DEBUG): $(OBJ_SERVER_DEBUG)
# 	$(CC) $(CFLAGS) $(DBGFLAGS) $(OBJ_SERVER_DEBUG) -o $@ $(LINKFLAGS)

# $(TARGET_CLIENT_TEST): $(OBJ_CLIENT_TEST)
# 	$(CC) $(CFLAGS) $(TESTFLAGS) $(OBJ_CLIENT_TEST) -o $@ $(LINKFLAGS)

# $(TARGET_SERVER_TEST): $(OBJ_SERVER_TEST)
# 	$(CC) $(CFLAGS) $(TESTFLAGS)  $(OBJ_SERVER_TEST) -o $@ $(LINKFLAGS)

# phony rules
.PHONY: makedir
makedir:
	@mkdir -p $(BIN_PATH) $(OBJ_PATH) $(DBG_PATH) $(TEST_PATH)

.PHONY: dependencies
dependencies:
	@echo "\n\n *** No dependencies to build for ATFTP *** \n\n"


.PHONY: all
all: makedir $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: debug
debug: makedir $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: test
test: makedir $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: install
install:
	mkdir -p $(INSTALL_PATH)/lib $(INSTALL_PATH)/include
	cp -f $(BIN_PATH)/*.so $(INSTALL_PATH)/lib
	cp -f *_api*.h $(INSTALL_PATH)/include

# TODO: remove only what we've installed
.PHONY: uninstall
uninstall:
	rm -rf $(INSTALL_PATH)/lib $(INSTALL_PATH)/include

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)

.PHONY: distclean
distclean:
	@echo CLEAN $(DISTCLEAN_LIST)
	@rm -f $(DISTCLEAN_LIST)
	@rm -rf $(BIN_PATH) $(OBJ_PATH) $(DBG_PATH)