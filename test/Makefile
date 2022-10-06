include config.mk

# path macros
BIN_PATH := bin
SRC_PATH := src
REPORT_PATH := report

DEPS := atftp

# compile macros
TARGET_NAME_CLIENT := unity_test_tftp_client
TARGET_CLIENT := $(BIN_PATH)/$(TARGET_NAME_CLIENT)

TARGET_NAME_SERVER := unity_test_tftp_server
TARGET_SERVER := $(BIN_PATH)/$(TARGET_NAME_SERVER)

# src files & obj files
SRC_CLIENT := $(UNITY_ROOT)/src/unity.c $(SRC_PATH)/TestTftp.c $(SRC_PATH)/Tftp_Runner.c
OBJ_CLIENT := $(addsuffix .o, $(basename $(SRC_CLIENT)))

SRC_SERVER := $(UNITY_ROOT)/src/unity.c $(SRC_PATH)/TestTftpd.c $(SRC_PATH)/Tftpd_Runner.c
OBJ_SERVER := $(addsuffix .o, $(basename $(SRC_SERVER)))

LIBS_CLIENT := $(LDLIBS) -ltftp
LIBS_SERVER := $(LDLIBS) -ltftpd

# clean files list
CLEAN_LIST := $(OBJ_CLIENT) \
			  $(OBJ_SERVER) \
			  $(TARGET_CLIENT) 	\
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
$(TARGET_CLIENT): $(OBJ_CLIENT)
	$(CC) -o $@ $(OBJ_CLIENT) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(INCFLAGS)

$(TARGET_SERVER): $(OBJ_SERVER)
	$(CC) -o $@ $(OBJ_SERVER) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(INCFLAGS)

atftp:
	cd .. && $(MAKE) $(DEP_RULE) -j$(shell echo $$((`nproc`))) && \
	$(MAKE) install DESTDIR=$(DEP_PATH)

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

.PHONY: deps
deps: $(DEPS)

.PHONY: debugdeps
debugdeps: $(DEPS)

.PHONY: testdeps
testdeps: $(DEPS)

.PHONY: all
all: makedir $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: debug
debug: makedir $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: runtests
runtests: runclient runserver

.PHONY: runclient
runclient:
	./$(TARGET_CLIENT)

.PHONY: runserver
runserver:
	./$(TARGET_SERVER)

.PHONY: report
report: 
	cd .. && $(MAKE) clean && cd -
	$(MAKE) clean
	$(MAKE) testdeps
	$(MAKE) all
	$(MAKE) runtests
	@mkdir -p $(REPORT_PATH)
	find .. -name "*.gcov" -exec gcov {} --object-directory ../obj \;
	lcov --capture --directory ../obj --output-file $(REPORT_PATH)/coverage.info
	genhtml $(REPORT_PATH)/coverage.info --output-directory $(REPORT_PATH)

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)