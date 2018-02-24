#
# Copyright (C) 2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=chron
PKG_RELEASE:=1.0

PKG_MAINTAINER:=nick <nicklgw@aliyun.com>
PKG_LICENSE:=LGPL-2.1

include $(INCLUDE_DIR)/package-seccomp.mk
include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/chron
	SECTION:=leedarson
	CATEGORY:=Leedarson
	TITLE:=scheduled utility
	DEPENDS:=+libjson-c +libsqlite3 +libuci +libubox +libubus +libblobmsg-json
endef

define Package/chron/description
	daemon to execute scheduled commands
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/chron/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/chron $(1)/usr/bin/
	$(INSTALL_BIN) ./files/chron.init $(1)/etc/init.d/chron
	$(INSTALL_CONF) ./files/chron.config $(1)/etc/config/chron
endef

$(eval $(call BuildPackage,chron))

