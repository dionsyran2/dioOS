LIB_PACKAGES += build_xkeyboard_config

XKB_CONFIG_VERSION = xkeyboard-config-2.38
XKB_CONFIG_URL = https://www.x.org/releases/individual/data/xkeyboard-config/$(XKB_CONFIG_VERSION).tar.xz

build_xkeyboard_config: build_xorg
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_XKEYBOARD_CONFIG),y)
	$(MAKE) make_xkeyboard_config
endif

else
	$(MAKE) make_xkeyboard_config
endif
endif

make_xkeyboard_config:
	@echo "Installing XKB keyboard layouts..."
	@if [ ! -f $(TMPDIR)/$(XKB_CONFIG_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(XKB_CONFIG_URL); fi
	@if [ ! -d $(TMPDIR)/$(XKB_CONFIG_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XKB_CONFIG_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(XKB_CONFIG_VERSION) && \
	meson setup build --prefix=/usr
	
	ninja -C $(TMPDIR)/$(XKB_CONFIG_VERSION)/build
	DESTDIR=$(DISKDIR) ninja -C $(TMPDIR)/$(XKB_CONFIG_VERSION)/build install
