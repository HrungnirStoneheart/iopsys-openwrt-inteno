#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=mtd-utils
PKG_VERSION:=1.4.5
PKG_RELEASE:=1
ifeq ($(CONFIG_BCM_OPEN),y)
PKG_SOURCE_URL:=http://ihgsp.inteno.se/git/mtd-utils
else
PKG_SOURCE_URL:=git@ihgsp.inteno.se:mtd-utils
endif
PKG_SOURCE_PROTO:=git
PKG_SOURCE_VERSION:=3a0d8a01d91e0e80bc59d6320e91c07383f0ede2
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION)-$(PKG_SOURCE_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_CAT:=zcat

HOST_BUILD_DIR:=$(BUILD_DIR_HOST)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/host-build.mk

HOST_CFLAGS += -I$(STAGING_DIR_HOST)/include/e2fsprogs
CFLAGS := $(HOST_CFLAGS) -I$(HOST_BUILD_DIR)/include -L$(HOST_BUILD_DIR) -L$(STAGING_DIR_HOST)/lib -DNO_NATIVE_SUPPORT
ifneq ($(HOST_OS),Linux)
CFLAGS += -Dloff_t=off_t -D__BYTE_ORDER=BYTE_ORDER -include getline.h -include endian.h -I$(CURDIR)/include -include fls.h
endif

MTD_MAKEOPTS = \
	CFLAGS="$(CFLAGS)" \
	LDFLAGS="$(HOST_LDFLAGS) $(HOST_STATIC_LINKING)" \
	WITHOUT_LZO=1 WITHOUT_XATTR=1 \
	LZMA_STATIC_LIB="$(STAGING_DIR_HOST)/lib/liblzma.a" \
	SUBDIRS="" \
	BUILDDIR="$(HOST_BUILD_DIR)"

define Host/Compile
	$(MAKE) -C $(HOST_BUILD_DIR)/lib \
		$(MTD_MAKEOPTS) \
		TARGETS="libmtd.a libcrc32.a" \
		LIBS="libmtd libcrc32"
	$(MAKE) -C $(HOST_BUILD_DIR) \
		$(MTD_MAKEOPTS) \
		TARGETS=mkfs.jffs2
	$(MAKE) -C $(HOST_BUILD_DIR)/ubi-utils \
		$(MTD_MAKEOPTS) \
		TARGETS=ubinize
	$(MAKE) -C $(HOST_BUILD_DIR)/mkfs.ubifs \
		$(MTD_MAKEOPTS) \
		BUILDDIR="$(HOST_BUILD_DIR)/mkfs.ubifs"
endef

define Host/Install
	$(CP) \
		$(HOST_BUILD_DIR)/mkfs.jffs2 \
		$(HOST_BUILD_DIR)/mkfs.ubifs/mkfs.ubifs \
		$(HOST_BUILD_DIR)/ubinize \
		$(STAGING_DIR_HOST)/bin/
endef

define Host/Clean
	rm -f $(STAGING_DIR_HOST)/bin/{mkfs.jffs2,mkfs.ubifs,ubinize}
endef

$(eval $(call HostBuild))
