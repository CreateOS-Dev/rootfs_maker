APP_NAME := httpd
APP_SRC := $(CURDIR)/main.c
ROOTFS_MAKER ?= $(abspath ../../..)
SDK_ROOT ?= $(ROOTFS_MAKER)/../sdk
APP_MAIN_DEPS := $(SDK_ROOT)/include/createos/http.h \
	$(SDK_ROOT)/include/createos/network.h
include $(SDK_ROOT)/app.mk
