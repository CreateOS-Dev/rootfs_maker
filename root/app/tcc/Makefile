APP_NAME := tcc
TCC_SOURCE ?= $(ROOTFS_MAKER)/../kernel/lib/third_party/tinycc
APP_SRC := $(TCC_SOURCE)/tcc.c
TCC_PORT := $(CURDIR)/port
TCC_BUILD := $(BUILD_DIR)/tcc
TCC_RUNTIME := $(TCC_BUILD)/runtime
TCC_FLAGS := -DTCC_TARGET_X86_64 -DCONFIG_TCC_STATIC \
	-DCONFIG_TCCBOOT -U__x86_64__ \
	-Dmain=createos_tcc_main \
	-DCONFIG_TCCDIR=\"/usr/lib/tcc\" \
	-DCONFIG_TCC_LIBPATHS=\"/usr/lib/tcc\" \
	-DCONFIG_TCC_CRTPREFIX=\"/usr/lib/tcc\" \
	-DCONFIG_TCC_SYSINCLUDEPATHS=\"/usr/include\"
TCC_INCLUDES := -I$(TCC_PORT)/include -I$(ROOTFS_MAKER)/lib \
	-I$(ROOTFS_MAKER)/../sdk/include -I$(TCC_SOURCE)
TCC_SOURCE_FILES := $(wildcard $(TCC_SOURCE)/*.c)
APP_CFLAGS := $(TCC_INCLUDES) $(TCC_FLAGS) -DCREATEOS_USERLAND
APP_CFLAGS += -include $(TCC_PORT)/tcc_compat.h
APP_MAIN_DEPS := $(TCC_SOURCE_FILES) \
	$(TCC_SOURCE)/tcc.h $(TCC_SOURCE)/config.h \
	$(TCC_SOURCE)/tcclib.h $(TCC_SOURCE)/libtcc.h \
	$(CURDIR)/Makefile \
	$(TCC_PORT)/include/stdio.h $(TCC_PORT)/include/stdlib.h \
	$(TCC_PORT)/include/string.h $(TCC_PORT)/include/unistd.h \
	$(TCC_PORT)/include/setjmp.h $(TCC_PORT)/tcc_compat.h
APP_OBJECTS := $(TCC_BUILD)/tcc_compat.o
APP_CLEAN_EXTRA := $(TCC_BUILD)
APP_LDFLAGS := --gc-sections

include $(SDK_ROOT)/app.mk

TCC_RUNTIME_CFLAGS := -m64 -ffreestanding -fno-pic -fno-stack-protector \
	-mno-red-zone -fno-asynchronous-unwind-tables -fno-unwind-tables
TCC_RUNTIME_OBJECTS := $(TCC_RUNTIME)/libtcc1.o \
	$(TCC_RUNTIME)/alloca86_64.o $(TCC_RUNTIME)/alloca86_64-bt.o \
	$(TCC_RUNTIME)/va_list.o

$(TCC_RUNTIME)/libtcc1.o: $(TCC_SOURCE)/lib/libtcc1.c
	mkdir -p $(@D)
	$(CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(TCC_RUNTIME)/alloca86_64.o: $(TCC_SOURCE)/lib/alloca86_64.S
	mkdir -p $(@D)
	$(CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(TCC_RUNTIME)/alloca86_64-bt.o: $(TCC_SOURCE)/lib/alloca86_64-bt.S
	mkdir -p $(@D)
	$(CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(TCC_RUNTIME)/va_list.o: $(TCC_SOURCE)/lib/va_list.c
	mkdir -p $(@D)
	$(CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(TCC_RUNTIME)/libtcc1.a: $(TCC_RUNTIME_OBJECTS)
	$(AR) rcs $@ $^

$(TCC_RUNTIME)/crt1.o: $(ROOTFS_MAKER)/lib/command_start.S
	mkdir -p $(@D)
	$(CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(TCC_RUNTIME)/empty-crt.o: $(TCC_PORT)/tcc_empty.S
	mkdir -p $(@D)
	$(CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(TCC_RUNTIME)/crti.o $(TCC_RUNTIME)/crtn.o: $(TCC_RUNTIME)/empty-crt.o
	cp $< $@

$(TCC_BUILD)/tcc_compat.o: $(TCC_PORT)/tcc_compat.c $(APP_MAIN_DEPS) \
		$(ROOTFS_MAKER)/lib/user_syscall.h
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(APP_CFLAGS) -ffunction-sections -fdata-sections \
		-c $< -o $@

$(APP_OUT): $(TCC_BUILD)/tcc_compat.o $(TCC_RUNTIME)/libtcc1.a \
		$(TCC_RUNTIME)/crt1.o $(TCC_RUNTIME)/crti.o $(TCC_RUNTIME)/crtn.o
