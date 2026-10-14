APP_NAME := lua
APP_SRC := $(CURDIR)/main.c
ROOTFS_MAKER ?= $(abspath ../../..)
SDK_ROOT ?= $(ROOTFS_MAKER)/../sdk
LUA_ROOT := $(ROOTFS_MAKER)/../kernel/lib/third_party/lua-5.4.8
LUA_SRC := $(LUA_ROOT)/src
LUA_BUILD = $(BUILD_DIR)/lua
LUA_CORE_NAMES := lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem \
	lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio
LUA_LIB_NAMES := lauxlib lbaselib lcorolib linit liolib loslib lmathlib \
	ldblib loadlib lstrlib ltablib lutf8lib
LUA_NAMES := $(LUA_CORE_NAMES) $(LUA_LIB_NAMES)
LUA_OBJECTS = $(addprefix $(LUA_BUILD)/,$(addsuffix .o,$(LUA_NAMES))) \
	$(LUA_BUILD)/lua_compat.o $(LUA_BUILD)/lua_math.o
LUA_COMPILER_OUT = $(BUILD_DIR)/app/luac.APPL
APP_OBJECTS = $(LUA_OBJECTS)
APP_MAIN_DEPS := $(LUA_SRC)/lua.h $(LUA_SRC)/lauxlib.h $(LUA_SRC)/lualib.h \
	$(LUA_SRC)/luaconf.h \
	$(CURDIR)/compat/lua_compat.h \
	$(CURDIR)/compat/include/stdio.h \
	$(ROOTFS_MAKER)/lib/user_syscall.h
LUA_COMPAT_DEPS := $(CURDIR)/compat/lua_compat.h \
	$(LUA_SRC)/luaconf.h \
	$(CURDIR)/compat/include/stdio.h \
	$(CURDIR)/compat/include/math.h \
	$(CURDIR)/compat/include/locale.h \
	$(ROOTFS_MAKER)/lib/user_syscall.h \
	$(ROOTFS_MAKER)/lib/stdlib.h \
	$(ROOTFS_MAKER)/lib/time.h \
	$(ROOTFS_MAKER)/lib/errno.h
APP_CLEAN_EXTRA = $(LUA_BUILD) $(LUA_COMPILER_OUT)
APP_CFLAGS := -I$(CURDIR)/compat/include \
	-I$(ROOTFS_MAKER)/../kernel/lib/third_party/lua-5.4.8/src \
	-include $(CURDIR)/compat/lua_compat.h \
	-include $(CURDIR)/compat/include/stdio.h \
	-Dl_signalT=int \
	-Dl_randomizePivot=createos_lua_randomize_pivot \
	-DCREATEOS_LUA_FORMAT \
	-DLUA_COMPAT_5_3
APP_CFLAGS += -Wno-return-type
APP_LDFLAGS := --gc-sections
include $(SDK_ROOT)/app.mk

.PHONY: luac
luac: $(LUA_COMPILER_OUT)

$(LUA_COMPILER_OUT): $(LUA_BUILD)/luac.o $(LUA_BUILD)/luac_compat.o \
		$(LUA_OBJECTS) $(SDK_LIBS) $(APP_LD)
	mkdir -p $(@D)
	$(LD) $(LDFLAGS) $(APP_LDFLAGS) -e user_command_start -o $@ \
		$(LUA_BUILD)/luac.o $(LUA_BUILD)/luac_compat.o \
		-u user_command_start $(LUA_OBJECTS) \
		--whole-archive $(SDK_LIBS) --no-whole-archive \
		--start-group $(LIBGCC) --end-group

$(LUA_BUILD)/luac.o: $(LUA_SRC)/luac.c $(LUA_COMPAT_DEPS)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(APP_CFLAGS) -DluaL_newstate=createos_luac_newstate \
		-ffunction-sections -fdata-sections -c $< -o $@

$(LUA_BUILD)/luac_compat.o: $(CURDIR)/compat/luac_compat.c \
		$(LUA_SRC)/lua.h $(CURDIR)/compat/lua_compat.h
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(APP_CFLAGS) -c $< -o $@

.PHONY: clean-luac
clean-luac:
	rm -f $(LUA_COMPILER_OUT) $(LUA_BUILD)/luac.o \
		$(LUA_BUILD)/luac_compat.o

$(LUA_BUILD)/%.o: $(LUA_SRC)/%.c $(LUA_COMPAT_DEPS)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(APP_CFLAGS) -ffunction-sections -fdata-sections \
		-c $< -o $@

$(LUA_BUILD)/lauxlib.o: $(LUA_SRC)/lauxlib.c $(LUA_COMPAT_DEPS)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(APP_CFLAGS) -DluaL_loadfilex=luaL_loadfilex_unused \
		-ffunction-sections -fdata-sections -c $< -o $@

$(LUA_BUILD)/lua_compat.o: $(CURDIR)/compat/lua_compat.c \
		$(LUA_COMPAT_DEPS)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(APP_CFLAGS) -c $< -o $@

$(LUA_BUILD)/lua_math.o: $(CURDIR)/compat/lua_math.c \
		$(CURDIR)/compat/include/math.h $(CURDIR)/compat/lua_compat.h
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(APP_CFLAGS) -c $< -o $@
