# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2023 ImmortalWrt.org
# Based on https://github.com/douglarek/mihomo-openwrt at 68f9c94de324588ee6b1b15d120d0927cea78b45.

include $(TOPDIR)/rules.mk

PKG_NAME:=mihomo
PKG_VERSION:=1.19.32
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://codeload.github.com/MetaCubeX/mihomo/tar.gz/v$(PKG_VERSION)?
PKG_HASH:=ab130b7fab3893d01aa44c0d39be34a26da716c1d36832fbac9fa054a1d862a0

PKG_LICENSE:=GPL-3.0-only
PKG_LICENSE_FILES:=LICENSE
PKG_BUILD_DEPENDS:=golang/host
PKG_BUILD_PARALLEL:=1
PKG_BUILD_FLAGS:=no-mips16

GO_PKG:=github.com/metacubex/mihomo
GO_PKG_BUILD_PKG:=$(GO_PKG)
GO_PKG_LDFLAGS_X:= \
        $(GO_PKG)/constant.Version=v$(PKG_VERSION) \
        $(GO_PKG)/constant.BuildTime=$(shell date -u -d @$(SOURCE_DATE_EPOCH) '+%F-%T-UTC')
GO_PKG_TAGS:=with_gvisor

include $(INCLUDE_DIR)/package.mk
include $(TOPDIR)/feeds/packages/lang/golang/golang-package.mk

define Package/mihomo
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=Web Servers/Proxies
  TITLE:=A rule-based tunnel in Go
  URL:=https://github.com/MetaCubeX/mihomo
  DEPENDS:=$(GO_ARCH_DEPENDS) +ca-bundle +kmod-tun +kmod-inet-diag +kmod-netlink-diag
endef

define Package/mihomo/description
  Mihomo is a cross-platform rule-based proxy that supports TUN and multiple
  proxy protocols. This package is disabled until explicitly configured.
endef

define Package/mihomo/conffiles
/etc/mihomo/config.yaml
/etc/config/mihomo
endef

define Package/mihomo/install
	$(call GoPackage/Package/Install/Bin,$(1))
	$(INSTALL_DIR) $(1)/etc/mihomo/
	$(INSTALL_CONF) $(PKG_BUILD_DIR)/docs/config.yaml $(1)/etc/mihomo/example.yaml
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) $(CURDIR)/files/mihomo.config $(1)/etc/config/mihomo
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(CURDIR)/files/mihomo.init $(1)/etc/init.d/mihomo
endef

$(eval $(call GoBinPackage,mihomo))
$(eval $(call BuildPackage,mihomo))
