include $(TOPDIR)/rules.mk

PKG_NAME:=vep1400-support
PKG_VERSION:=1.0
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define Package/vep1400-support
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Dell EMC VEP1400 / Edge 610 board bring-up and MDIO tool
  DEPENDS:=@TARGET_x86 +kmod-ixgbe +i2c-tools +kmod-i2c-ismt +kmod-i2c-core
endef

define Package/vep1400-support/description
 Board bring-up scripts and clean-room MDIO access tool for the
 Dell EMC Networking VEP1400 and VMware SD-WAN Edge 610 appliances.
 Configures the Marvell 88E6190 switch, SerDes 2.5G uplinks, and SFP cages.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) \
		$(PKG_BUILD_DIR)/vep1400-mdio-tool.c \
		-o $(PKG_BUILD_DIR)/vep1400-mdio-tool
endef

define Package/vep1400-support/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/vep1400-mdio-tool $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/usr/libexec/vep1400
	$(INSTALL_BIN) ./files/switch-init $(1)/usr/libexec/vep1400/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/vep1400-switch.init $(1)/etc/init.d/vep1400-switch
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/99_vep1400_network $(1)/etc/uci-defaults/99_vep1400_network
endef

$(eval $(call BuildPackage,vep1400-support))
