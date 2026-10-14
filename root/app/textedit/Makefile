APP_NAME := textedit
APP_SRC := $(CURDIR)/main.c
ROOTFS_MAKER ?= $(abspath ../../..)
SDK_ROOT ?= $(ROOTFS_MAKER)/../sdk
LUA_ROOT := $(ROOTFS_MAKER)/../kernel/lib/third_party/lua-5.4.8
LUA_SRC := $(LUA_ROOT)/src
LUA_BUILD = $(BUILD_DIR)/textedit-lua
LUA_CORE_NAMES := lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem \
	lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio
LUA_LIB_NAMES := lauxlib lbaselib lcorolib linit liolib loslib lmathlib \
	ldblib loadlib lstrlib ltablib lutf8lib
LUA_NAMES := $(LUA_CORE_NAMES) $(LUA_LIB_NAMES)
LUA_OBJECTS = $(addprefix $(LUA_BUILD)/,$(addsuffix .o,$(LUA_NAMES))) \
	$(LUA_BUILD)/lua_compat.o $(LUA_BUILD)/lua_math.o
APP_OBJECTS = $(LUA_OBJECTS)
APP_MAIN_DEPS := $(LUA_SRC)/lua.h $(LUA_SRC)/lauxlib.h \
	$(LUA_SRC)/lualib.h $(LUA_SRC)/luaconf.h \
	$(ROOTFS_MAKER)/root/app/lua/compat/lua_compat.h \
	$(ROOTFS_MAKER)/root/app/lua/compat/include/stdio.h \
	$(ROOTFS_MAKER)/lib/user_syscall.h
LUA_COMPAT_DIR := $(ROOTFS_MAKER)/root/app/lua/compat
LUA_COMPAT_DEPS := $(LUA_COMPAT_DIR)/lua_compat.h \
	$(LUA_SRC)/luaconf.h \
	$(LUA_COMPAT_DIR)/include/stdio.h \
	$(LUA_COMPAT_DIR)/include/math.h \
	$(LUA_COMPAT_DIR)/include/locale.h \
	$(ROOTFS_MAKER)/lib/user_syscall.h \
	$(ROOTFS_MAKER)/lib/stdlib.h \
	$(ROOTFS_MAKER)/lib/time.h \
	$(ROOTFS_MAKER)/lib/errno.h
APP_CFLAGS += -I$(LUA_COMPAT_DIR)/include -I$(LUA_SRC) \
	-include $(LUA_COMPAT_DIR)/lua_compat.h \
	-include $(LUA_COMPAT_DIR)/include/stdio.h \
	-Dl_signalT=int \
	-Dl_randomizePivot=createos_lua_randomize_pivot \
	-DCREATEOS_LUA_FORMAT -DLUA_COMPAT_5_3 -Wno-return-type
APP_LDFLAGS += --gc-sections
APP_CLEAN_EXTRA = $(LUA_BUILD)
include $(SDK_ROOT)/app.mk

$(LUA_BUILD)/%.o: $(LUA_SRC)/%.c $(LUA_COMPAT_DEPS)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(APP_CFLAGS) -ffunction-sections -fdata-sections \
		-c $< -o $@

$(LUA_BUILD)/lauxlib.o: $(LUA_SRC)/lauxlib.c $(LUA_COMPAT_DEPS)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(APP_CFLAGS) -DluaL_loadfilex=luaL_loadfilex_unused \
		-ffunction-sections -fdata-sections -c $< -o $@

$(LUA_BUILD)/lua_compat.o: $(LUA_COMPAT_DIR)/lua_compat.c \
		$(LUA_COMPAT_DEPS)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(APP_CFLAGS) -c $< -o $@

$(LUA_BUILD)/lua_math.o: $(LUA_COMPAT_DIR)/lua_math.c \
		$(LUA_COMPAT_DIR)/include/math.h \
		$(LUA_COMPAT_DIR)/lua_compat.h
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(APP_CFLAGS) -c $< -o $@
