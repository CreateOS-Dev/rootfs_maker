
# Host tool compiler (for tools that run on the build system)
HOSTCC ?= gcc
HOSTCFLAGS ?= -Wall -Wextra -I./tools
PREFIX ?= /usr/local
DESTDIR ?=
TOOLCHAIN_PREFIX ?= ../build/toolchain/x86_64-elf
override CROSS_COMPILE := $(TOOLCHAIN_PREFIX)/bin/x86_64-elf-
BUILD_DIR ?= ../build/rootfs_maker
ROOTFS_DIR ?= ../build/rootfs
DISK_IMAGE ?= ../build/disk.img

# Cross-compiler for user applications
override CC := $(CROSS_COMPILE)gcc
CFLAGS := -m64 -ffreestanding -fno-pic -fno-stack-protector -mno-red-zone -I./lib -Wall -Wextra
override LD := $(CROSS_COMPILE)ld
LDFLAGS := -n -z noexecstack -T app.ld
OBJCOPY := $(CROSS_COMPILE)objcopy

# User library objects
USERLIB_OBJS := $(BUILD_DIR)/lib/syscalls.o $(BUILD_DIR)/lib/userlib.o \
	$(BUILD_DIR)/lib/posix.o
USER_API_HEADERS := $(wildcard lib/*.h lib/sys/*.h) \
	root/sys/network_startup.h \
	../kernel/header/version.h \
	../kernel/header/drivers/netdev.h ../kernel/header/drivers/netstack.h \
	../kernel/header/drivers/pci.h \
	../kernel/header/drivers/usb.h \
	../kernel/header/fs/fs.h ../kernel/header/fs/vfs.h
COMMAND_START_OBJ := $(BUILD_DIR)/command_start.o
NETWORK_STARTUP_OBJ := $(BUILD_DIR)/network_startup.o
MBEDTLS_ROOT := ../build/third_party/mbedtls-4.0.0
MBEDTLS_BUILD := ../build/mbedtls-4.0.0-x86_64-elf
MBEDTLS_STAMP := $(MBEDTLS_BUILD)/.createos-build
MBEDTLS_LIBS := $(MBEDTLS_BUILD)/library/libmbedtls.a \
	$(MBEDTLS_BUILD)/library/libmbedx509.a \
	$(MBEDTLS_BUILD)/library/libmbedcrypto.a \
	$(MBEDTLS_BUILD)/library/libtfpsacrypto.a
MBEDTLS_CFLAGS := -ffunction-sections -fdata-sections \
	-I$(MBEDTLS_ROOT)/include \
	-I$(MBEDTLS_ROOT)/tf-psa-crypto/include \
	-I$(MBEDTLS_ROOT)/tf-psa-crypto/drivers/builtin/include \
	-I$(MBEDTLS_ROOT)/tf-psa-crypto/drivers/everest/include \
	-I$(MBEDTLS_ROOT)/tf-psa-crypto/drivers/p256-m \
	-I$(MBEDTLS_BUILD)/library \
	-I$(MBEDTLS_BUILD)/tf-psa-crypto/core \
	-DMBEDTLS_CONFIG_FILE=\"$(abspath lib/createos_mbedtls_config.h)\" \
	-DTF_PSA_CRYPTO_CONFIG_FILE=\"$(abspath lib/createos_psa_crypto_config.h)\"

# Core system binaries
SHELL_SRC := root/prog/shell/shell.c
LOGIN_SRC := root/sys/login.c
INIT_SRC := root/sys/init.c
FIRSTBOOT_SRC := root/base/firstboot.c
APP_MAKEFILES := $(wildcard root/app/*/Makefile)
APP_NAMES := $(notdir $(patsubst %/Makefile,%,$(APP_MAKEFILES)))
LUA_COMPILER_APP := $(BUILD_DIR)/app/luac.APPL
CUSTOM_APPLS := $(wildcard root/appcustom/*.APPL) \
	$(wildcard root/appcustom/*.appl)

# Commands - each as separate binary
CMD_SRCS := $(filter-out root/commands/curl_http.c,$(wildcard root/commands/*.c))
CMD_NAMES := $(basename $(notdir $(CMD_SRCS)))

# Final binaries
APPS := sh login init firstboot $(CMD_NAMES) $(APP_NAMES)

all: check-toolchain $(BUILD_DIR)/sh.bin $(BUILD_DIR)/login.bin $(BUILD_DIR)/init.bin \
	$(BUILD_DIR)/firstboot.bin \
	$(addprefix $(BUILD_DIR)/,$(addsuffix .bin,$(CMD_NAMES))) \
	$(addprefix $(BUILD_DIR)/app/,$(addsuffix .APPL,$(APP_NAMES))) \
	$(LUA_COMPILER_APP)

.NOTPARALLEL: all

$(BUILD_DIR):
	mkdir -p $@

check-toolchain:
	@for tool in "$(CC)" "$(LD)"; do \
		if [ ! -x "$$tool" ]; then \
			printf 'Required CreateOS toolchain executable is missing: %s\n' \
				"$$tool" >&2; \
			printf 'Build the pinned local toolchain first with: make toolchain\n' >&2; \
			exit 1; \
		fi; \
	done
	@target=`"$(CC)" -dumpmachine` || exit $$?; \
	if [ "$$target" != x86_64-elf ]; then \
		printf 'Local toolchain target mismatch: expected x86_64-elf, got %s\n' \
			"$$target" >&2; exit 1; \
	fi

# User library
$(BUILD_DIR)/lib/syscalls.o: lib/syscalls.S lib/user_syscall.h Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lib/userlib.o: lib/userlib.c lib/user_syscall.h Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lib/posix.o: lib/posix.c lib/user_syscall.h lib/errno.h \
	lib/fcntl.h lib/dirent.h lib/sys/stat.h lib/sys/types.h lib/unistd.h Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(COMMAND_START_OBJ): lib/command_start.S Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(MBEDTLS_STAMP): ../scripts/build-mbedtls.sh \
		lib/createos_mbedtls_config.h lib/createos_psa_crypto_config.h | check-toolchain
	bash ../scripts/build-mbedtls.sh
	mkdir -p $(@D)
	touch $@

$(MBEDTLS_LIBS): $(MBEDTLS_STAMP)
	@test -s "$@" || { \
		printf 'Mbed TLS build did not produce required archive: %s\n' "$@" >&2; \
		exit 1; \
	}

$(BUILD_DIR)/curl_tls.o: root/net/curl_tls.c root/commands/curl_tls.h \
		$(USER_API_HEADERS) $(MBEDTLS_STAMP) Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(MBEDTLS_CFLAGS) -Iroot/commands -c $< -o $@

$(BUILD_DIR)/curl_http.o: root/commands/curl_http.c root/commands/curl_http.h \
		Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Shell
$(BUILD_DIR)/shell.o: root/prog/shell/shell.c root/prog/shell/shell_modules.h $(USER_API_HEADERS) Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell_prompt.o: root/prog/shell/shell_prompt.c root/prog/shell/shell_modules.h $(USER_API_HEADERS) Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell_exec.o: root/prog/shell/shell_exec.c root/prog/shell/shell_modules.h $(USER_API_HEADERS) Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell_history.o: root/prog/shell/shell_history.c root/prog/shell/shell_modules.h $(USER_API_HEADERS) Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sh.elf: $(BUILD_DIR)/shell.o $(BUILD_DIR)/shell_prompt.o $(BUILD_DIR)/shell_exec.o $(BUILD_DIR)/shell_history.o $(USERLIB_OBJS) app.ld Makefile
	$(LD) $(LDFLAGS) -o $@ $(BUILD_DIR)/shell.o $(BUILD_DIR)/shell_prompt.o $(BUILD_DIR)/shell_exec.o $(BUILD_DIR)/shell_history.o $(USERLIB_OBJS)

$(BUILD_DIR)/sh.bin: $(BUILD_DIR)/sh.elf
	cp $< $@

# Login
$(BUILD_DIR)/login.o: root/sys/login.c $(USER_API_HEADERS) Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/login.elf: $(BUILD_DIR)/login.o $(USERLIB_OBJS) app.ld Makefile
	$(LD) $(LDFLAGS) -o $@ $(BUILD_DIR)/login.o $(USERLIB_OBJS)

$(BUILD_DIR)/login.bin: $(BUILD_DIR)/login.elf
	cp $< $@

# Init (PID 1)
$(BUILD_DIR)/init.o: root/sys/init.c $(USER_API_HEADERS) Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(NETWORK_STARTUP_OBJ): root/sys/network_startup.c root/sys/network_startup.h \
		$(USER_API_HEADERS) Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -mno-sse -mno-sse2 -mno-mmx -c $< -o $@

$(BUILD_DIR)/init.elf: $(BUILD_DIR)/init.o $(NETWORK_STARTUP_OBJ) \
		$(USERLIB_OBJS) app.ld Makefile
	$(LD) $(LDFLAGS) -o $@ $(BUILD_DIR)/init.o $(NETWORK_STARTUP_OBJ) \
		$(USERLIB_OBJS)

$(BUILD_DIR)/init.bin: $(BUILD_DIR)/init.elf
	cp $< $@

# Firstboot setup
$(BUILD_DIR)/firstboot.o: root/base/firstboot.c $(USER_API_HEADERS) Makefile | check-toolchain
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/firstboot.elf: $(BUILD_DIR)/firstboot.o $(USERLIB_OBJS) app.ld Makefile
	$(LD) $(LDFLAGS) -o $@ $(BUILD_DIR)/firstboot.o $(USERLIB_OBJS)

$(BUILD_DIR)/firstboot.bin: $(BUILD_DIR)/firstboot.elf
	cp $< $@

# Each built-in application owns its project Makefile and build rules.
.PHONY: FORCE_APP_BUILD
FORCE_APP_BUILD:

TCC_SOURCE ?= ../kernel/lib/third_party/tinycc
TCC_SOURCE_READY := $(BUILD_DIR)/tcc-source-ready

.PHONY: FORCE_TCC_SOURCE_PREP
FORCE_TCC_SOURCE_PREP:

$(TCC_SOURCE_READY): FORCE_TCC_SOURCE_PREP ../scripts/build-tcc-host.sh
	mkdir -p $(@D)
	TCC_SOURCE="$(abspath $(TCC_SOURCE))" TCC_SOURCE_ONLY=1 \
		bash ../scripts/build-tcc-host.sh
	touch $@

$(BUILD_DIR)/app/tcc.APPL: $(TCC_SOURCE_READY)

$(BUILD_DIR)/app/%.APPL: FORCE_APP_BUILD root/app/%/Makefile \
		../sdk/app.mk app.ld Makefile | check-toolchain
	$(MAKE) -C root/app/$* APP_NAME=$* \
		ROOTFS_MAKER="$(abspath .)" \
		SDK_ROOT="$(abspath ../sdk)" \
		BUILD_DIR="$(abspath $(BUILD_DIR))" \
		TOOLCHAIN_PREFIX="$(abspath $(TOOLCHAIN_PREFIX))" \
		TCC_SOURCE="$(abspath $(TCC_SOURCE))"

.PHONY: FORCE_LUA_COMPILER_BUILD
FORCE_LUA_COMPILER_BUILD:

$(LUA_COMPILER_APP): FORCE_LUA_COMPILER_BUILD root/app/lua/Makefile \
		../sdk/app.mk app.ld Makefile | check-toolchain
	$(MAKE) -C root/app/lua luac APP_NAME=lua \
		ROOTFS_MAKER="$(abspath .)" \
		SDK_ROOT="$(abspath ../sdk)" \
		BUILD_DIR="$(abspath $(BUILD_DIR))" \
		TOOLCHAIN_PREFIX="$(abspath $(TOOLCHAIN_PREFIX))"

# Commands - each as separate binary, kept in the shared build directory.
define CMD_RULES
$(BUILD_DIR)/$(1).o: root/commands/$(1).c $(USER_API_HEADERS) \
	$$(if $$(filter curl,$(1)),root/commands/curl_http.h,) Makefile | check-toolchain
	mkdir -p $$(@D)
	$$(CC) $$(CFLAGS) $$(if $$(filter curl,$(1)),-Iroot/commands,) -D_start=command_legacy_start -c $$< -o $$@

$(BUILD_DIR)/$(1).elf: $(BUILD_DIR)/$(1).o $$(COMMAND_START_OBJ) $$(USERLIB_OBJS) app.ld Makefile $$(if $$(filter curl,$(1)),$(BUILD_DIR)/curl_http.o $(BUILD_DIR)/curl_tls.o $(MBEDTLS_LIBS),)
	$$(LD) $$(LDFLAGS) $$(if $$(filter curl,$(1)),--gc-sections,) -e user_command_start -o $$@ $(BUILD_DIR)/$(1).o $$(if $$(filter curl,$(1)),$(BUILD_DIR)/curl_http.o $(BUILD_DIR)/curl_tls.o,) $$(COMMAND_START_OBJ) $$(USERLIB_OBJS) $$(if $$(filter curl,$(1)),--start-group $(MBEDTLS_LIBS) --end-group $$(shell $$(CC) -print-libgcc-file-name),)

$(BUILD_DIR)/$(1).bin: $(BUILD_DIR)/$(1).elf
	cp $$< $$@
endef

$(foreach cmd,$(CMD_NAMES),$(eval $(call CMD_RULES,$(cmd))))

$(BUILD_DIR)/adduser.o: ../kernel/header/proc/user_name.h

# Create final binaries in rootfs
populate-rootfs: all
	rm -rf $(ROOTFS_DIR)
	mkdir -p $(ROOTFS_DIR)
	if [ -d ../rootfs ]; then cp -R ../rootfs/. $(ROOTFS_DIR)/; fi
	rm -rf $(ROOTFS_DIR)/app
	mkdir -p $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/sbin $(ROOTFS_DIR)/etc/keyboards $(ROOTFS_DIR)/etc/cursors $(ROOTFS_DIR)/etc/ssl $(ROOTFS_DIR)/lib $(ROOTFS_DIR)/usr/bin $(ROOTFS_DIR)/usr/sbin $(ROOTFS_DIR)/usr/lib $(ROOTFS_DIR)/usr/include/createos $(ROOTFS_DIR)/usr/lib/createos $(ROOTFS_DIR)/usr/lib/tcc $(ROOTFS_DIR)/usr/include/sys $(ROOTFS_DIR)/usr/share/createos/cursors $(ROOTFS_DIR)/usr/share/fonts $(ROOTFS_DIR)/home/root $(ROOTFS_DIR)/app/system/bin $(ROOTFS_DIR)/app/custom
	rm -f $(ROOTFS_DIR)/bin/*.bin
	rm -f $(ROOTFS_DIR)/bin/arc $(ROOTFS_DIR)/bin/arc.bin \
		$(ROOTFS_DIR)/usr/bin/arc
	rm -rf $(ROOTFS_DIR)/rootfs
	rm -rf $(ROOTFS_DIR)/usr/share/createos/cursors/*
	cp root/assets/cursors/*.srbmp $(ROOTFS_DIR)/usr/share/createos/cursors/
	cp root/assets/fonts/*.ttf root/assets/fonts/*.txt $(ROOTFS_DIR)/usr/share/fonts/
	$(MAKE) -C ../sdk install SDK_BUILD=$(abspath $(BUILD_DIR)/sdk) \
		TOOLCHAIN_PREFIX=$(abspath $(TOOLCHAIN_PREFIX)) \
		DESTDIR=$(abspath $(ROOTFS_DIR))
	cp lib/*.h $(ROOTFS_DIR)/usr/include/
	cp lib/sys/*.h $(ROOTFS_DIR)/usr/include/sys/
	cp ../kernel/lib/third_party/tinycc/include/stdarg.h \
		../kernel/lib/third_party/tinycc/include/stddef.h \
		../kernel/lib/third_party/tinycc/include/stdbool.h \
		../kernel/lib/third_party/tinycc/include/float.h \
		$(ROOTFS_DIR)/usr/include/
	cp $(BUILD_DIR)/sdk/lib/libcreateos-user.a \
		$(ROOTFS_DIR)/usr/lib/tcc/libc.a
	cp $(BUILD_DIR)/tcc/runtime/libtcc1.a \
		$(BUILD_DIR)/tcc/runtime/crt1.o \
		$(BUILD_DIR)/tcc/runtime/crti.o \
		$(BUILD_DIR)/tcc/runtime/crtn.o $(ROOTFS_DIR)/usr/lib/tcc/
	cp root/etc/keyboards/*.lyt $(ROOTFS_DIR)/etc/keyboards/
	cp root/etc/keyboards/default $(ROOTFS_DIR)/etc/keyboards/default
	cp root/etc/cursors/default $(ROOTFS_DIR)/etc/cursors/default
	cp root/etc/cursors/type $(ROOTFS_DIR)/etc/cursors/type
	cp root/etc/locale.conf $(ROOTFS_DIR)/etc/locale.conf
	cp root/etc/timezone $(ROOTFS_DIR)/etc/timezone
	cp root/etc/power.conf $(ROOTFS_DIR)/etc/power.conf
	chmod 644 $(ROOTFS_DIR)/etc/power.conf
	cp root/etc/ssl/cert.pem $(ROOTFS_DIR)/etc/ssl/cert.pem
	chmod 644 $(ROOTFS_DIR)/etc/ssl/cert.pem
	cp root/etc/network.conf $(ROOTFS_DIR)/etc/network.conf
	chmod 644 $(ROOTFS_DIR)/etc/network.conf
	# Core system binaries
	cp $(BUILD_DIR)/sh.elf $(ROOTFS_DIR)/bin/sh
	chmod +x $(ROOTFS_DIR)/bin/sh
	cp $(BUILD_DIR)/login.elf $(ROOTFS_DIR)/bin/login
	chmod +x $(ROOTFS_DIR)/bin/login
	cp $(BUILD_DIR)/init.elf $(ROOTFS_DIR)/sbin/init
	chmod +x $(ROOTFS_DIR)/sbin/init
	cp $(BUILD_DIR)/firstboot.elf $(ROOTFS_DIR)/bin/firstboot
	chmod +x $(ROOTFS_DIR)/bin/firstboot
	$(foreach app,$(APP_NAMES),cp $(BUILD_DIR)/app/$(app).APPL $(ROOTFS_DIR)/app/system/bin/$(app).APPL; chmod +x $(ROOTFS_DIR)/app/system/bin/$(app).APPL;)
	cp $(LUA_COMPILER_APP) $(ROOTFS_DIR)/app/system/bin/luac.APPL
	chmod +x $(ROOTFS_DIR)/app/system/bin/luac.APPL
	$(foreach app,$(CUSTOM_APPLS),cp $(app) $(ROOTFS_DIR)/app/custom/$(notdir $(app)); chmod +x $(ROOTFS_DIR)/app/custom/$(notdir $(app));)
	cp root/app/system/*.link $(ROOTFS_DIR)/app/system/
	# Commands
	$(foreach cmd,$(CMD_NAMES),rm -f $(ROOTFS_DIR)/bin/$(cmd); cp $(BUILD_DIR)/$(cmd).bin $(ROOTFS_DIR)/bin/$(cmd).bin; chmod +x $(ROOTFS_DIR)/bin/$(cmd).bin;)
	# Create basic configuration files
	test -f $(ROOTFS_DIR)/etc/passwd || printf '%s\n' 'root::0:0:root:/home/root:/bin/sh' > $(ROOTFS_DIR)/etc/passwd
	test -f $(ROOTFS_DIR)/etc/group || printf '%s\n' 'root:x:0:' > $(ROOTFS_DIR)/etc/group
	test -f $(ROOTFS_DIR)/etc/users.conf || printf '%s\n' '# Accounts are created by firstboot.' > $(ROOTFS_DIR)/etc/users.conf
	test -f $(ROOTFS_DIR)/etc/sysconfig || printf '%s\n' 'goddo_locked=0' > $(ROOTFS_DIR)/etc/sysconfig

# Build host tools for image creation and inspection.
$(BUILD_DIR)/host/mkfs_cfs5: ./tools/mkfs_cfs5.c
	mkdir -p $(@D)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

$(BUILD_DIR)/host/copy_to_cfs5: ./tools/copy_to_cfs5.c
	mkdir -p $(@D)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

host-tools: $(BUILD_DIR)/host/mkfs_cfs5 $(BUILD_DIR)/host/copy_to_cfs5

$(BUILD_DIR)/host/cfs5_to_fat32: ./tools/cfs5_to_fat32.c ./tools/cfs5.h
	mkdir -p $(@D)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

$(BUILD_DIR)/host/inspect_cfs5: ./tools/inspect_cfs5.c ./tools/cfs5.h
	mkdir -p $(@D)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

$(BUILD_DIR)/host/cfs5tool: ./tools/cfs5tool.c ./tools/cfs5.h
	mkdir -p $(@D)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $< -lm

host-tools: $(BUILD_DIR)/host/mkfs_cfs5 $(BUILD_DIR)/host/copy_to_cfs5 \
	$(BUILD_DIR)/host/cfs5_to_fat32 $(BUILD_DIR)/host/inspect_cfs5 \
	$(BUILD_DIR)/host/cfs5tool

cfs5-to-fat32: $(BUILD_DIR)/host/cfs5_to_fat32
	@echo "Usage: $(BUILD_DIR)/host/cfs5_to_fat32 source.cfs5 formatted-fat32.img"

install-host-tools: host-tools
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $(BUILD_DIR)/host/mkfs_cfs5 $(DESTDIR)$(PREFIX)/bin/
	cp $(BUILD_DIR)/host/copy_to_cfs5 $(DESTDIR)$(PREFIX)/bin/
	cp $(BUILD_DIR)/host/cfs5_to_fat32 $(DESTDIR)$(PREFIX)/bin/
	cp $(BUILD_DIR)/host/inspect_cfs5 $(DESTDIR)$(PREFIX)/bin/
	cp $(BUILD_DIR)/host/cfs5tool $(DESTDIR)$(PREFIX)/bin/

uninstall-host-tools:
	rm -f $(DESTDIR)$(PREFIX)/bin/mkfs_cfs5
	rm -f $(DESTDIR)$(PREFIX)/bin/copy_to_cfs5
	rm -f $(DESTDIR)$(PREFIX)/bin/cfs5_to_fat32
	rm -f $(DESTDIR)$(PREFIX)/bin/inspect_cfs5
	rm -f $(DESTDIR)$(PREFIX)/bin/cfs5tool

# Create the persistent root filesystem image. This is explicit so normal
# kernel rebuilds and `make run` reuse the existing disk without repacking it.
disk-image: populate-rootfs disk-image-from-stage

disk-image-from-stage: host-tools
	$(BUILD_DIR)/host/mkfs_cfs5 $(DISK_IMAGE) 16384
	$(BUILD_DIR)/host/copy_to_cfs5 $(DISK_IMAGE) $(ROOTFS_DIR) /

reset-firstboot:
	rm -f $(ROOTFS_DIR)/etc/.firstboot_done

clean:
	rm -rf ../build/rootfs_maker ../build/rootfs

.PHONY: all check-toolchain populate-rootfs clean disk-image disk-image-from-stage reset-firstboot \
	host-tools install-host-tools uninstall-host-tools cfs5-to-fat32