LIB_PACKAGES += build_input_kbd

XF86_INPUT_KBD_VERSION = xf86-input-keyboard-1.9.0
XF86_INPUT_KBD_URL = https://www.x.org/releases/individual/driver/$(XF86_INPUT_KBD_VERSION).tar.bz2

build_input_kbd: build_xorg build_libudev
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_INPUT_KBD),y)
	$(MAKE) make_input_kbd
endif

else
	$(MAKE) make_input_kbd
endif
endif

make_input_kbd:
	@echo "Building xf86-input-keyboard..."
	@if [ ! -f $(TMPDIR)/$(XF86_INPUT_KBD_VERSION).tar.bz2 ]; then wget -P $(TMPDIR) $(XF86_INPUT_KBD_URL); fi
	@if [ ! -d $(TMPDIR)/$(XF86_INPUT_KBD_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XF86_INPUT_KBD_VERSION).tar.bz2; fi
	
	cd $(TMPDIR)/$(XF86_INPUT_KBD_VERSION) && \
	PKG_CONFIG_DIR="" \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib" \
	CPPFLAGS="-I$(DISKDIR)/usr/include" \
	CFLAGS="-I$(DISKDIR)/usr/include" \
	./configure --host=$(TARGET_ARCH) --prefix=/usr --with-pkgconfigdir=/usr/share/pkgconfig
	
	$(MAKE) -C $(TMPDIR)/$(XF86_INPUT_KBD_VERSION)
	$(MAKE) -C $(TMPDIR)/$(XF86_INPUT_KBD_VERSION) DESTDIR=$(DISKDIR) pkgconfigdir=/usr/lib/pkgconfig install

	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/usr/lib/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/usr/lib/pkgconfig/ 2>/dev/null || true