include config.mk

# path macros
OUT_PATH := lib
OBJ_PATH := obj
SRC_PATH := ./

# compile macros
# TARGET_NAME_CLIENT := libtftp.a libtftp.so
TARGET_NAME_CLIENT := libtftp.a
TARGET_CLIENT := $(addprefix $(OUT_PATH)/, $(TARGET_NAME_CLIENT))

# TARGET_NAME_SERVER := libtftpd.a libtftpd.so
TARGET_NAME_SERVER := libtftpd.a
TARGET_SERVER := $(addprefix $(OUT_PATH)/, $(TARGET_NAME_SERVER))

# src files & obj files
SRC_CLIENT := tftp.c tftp_io.c atftp_logger.c options.c tftp_def.c tftp_file.c
OBJ_CLIENT := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC_CLIENT)))))
OBJ_CLIENT_DEBUG := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC_CLIENT)))))
OBJ_CLIENT_TEST := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC_CLIENT)))))

SRC_SERVER := tftpd.c atftp_logger.c options.c tftp_io.c tftp_def.c tftpd_file.c tftpd_list.c
OBJ_SERVER := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC_SERVER)))))
OBJ_SERVER_DEBUG := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC_SERVER)))))
OBJ_SERVER_TEST := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC_SERVER)))))

# clean files list    
CLEAN_LIST := $(OBJ_CLIENT) \
			  $(OBJ_SERVER) \
			  $(TARGET_CLIENT) \
			  $(TARGET_SERVER) \
              $(DISTCLEAN_LIST)

# default rule
default: all

# non-phony targets
$(OUT_PATH)/libtftp.a: $(OBJ_CLIENT)
	@echo "Linking $@"
	$(AR) $(ARFLAGS) $@ $(OBJ_CLIENT)

$(OUT_PATH)/libtftp.so: $(OBJ_CLIENT)
	@echo "Linking $@"
	$(CC) -o $@ $(LINKFLAGS) $(OBJ_CLIENT) $(CFLAGS)

$(OUT_PATH)/libtftpd.a: $(OBJ_SERVER)
	@echo "Linking $@"
	$(AR) $(ARFLAGS) $@ $(OBJ_SERVER)

$(OUT_PATH)/libtftpd.so: $(OBJ_SERVER)
	@echo "Linking $@"
	$(CC) -o $@ $(LINKFLAGS) $(OBJ_SERVER) $(CFLAGS)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c*
	@echo "Building $<"
	$(CC) $(COBJFLAGS) -o $@ $<

# phony rules
.PHONY: makedir
makedir:
	@mkdir -p $(OUT_PATH) $(OBJ_PATH) $(DBG_PATH) $(TEST_PATH)

.PHONY: deps
deps:
	@echo "\n\n *** No dependencies to build for ATFTP *** \n\n"

.PHONY: all
all: makedir $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: debug
debug: makedir $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: test
test: makedir $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: install
install:
	@echo "\n\n *** Installing ATFTP to $(DESTDIR) *** \n\n"
	mkdir -p $(DESTDIR)/lib $(DESTDIR)/include
	cp -f $(TARGET_CLIENT) $(TARGET_SERVER) $(DESTDIR)/lib
	cp -f tftp*_api*.h $(DESTDIR)/include

#TODO: create uninstall rule

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)
	@rm -rf $(OUT_PATH) $(OBJ_PATH)