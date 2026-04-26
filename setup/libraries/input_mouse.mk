LIB_PACKAGES += build_input_mouse

XF86_INPUT_MOUSE_VERSION = xf86-input-mouse-1.9.3
XF86_INPUT_MOUSE_URL = https://www.x.org/releases/individual/driver/$(XF86_INPUT_MOUSE_VERSION).tar.bz2

build_input_mouse: build_xorg build_libudev
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_INPUT_MOUSE),y)
	$(MAKE) make_input_mouse
endif

else
	$(MAKE) make_input_mouse
endif
endif

make_input_mouse:
	@echo "Building xf86-input-mouse..."
	@if [ ! -f $(TMPDIR)/$(XF86_INPUT_MOUSE_VERSION).tar.bz2 ]; then wget -P $(TMPDIR) $(XF86_INPUT_MOUSE_URL); fi
	@if [ ! -d $(TMPDIR)/$(XF86_INPUT_MOUSE_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XF86_INPUT_MOUSE_VERSION).tar.bz2; fi
	
	cd $(TMPDIR)/$(XF86_INPUT_MOUSE_VERSION) && \
	PKG_CONFIG_DIR="" \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib" \
	CPPFLAGS="-I$(DISKDIR)/usr/include" \
	CFLAGS="-I$(DISKDIR)/usr/include" \
	./configure --host=$(TARGET_ARCH) --prefix=/usr --with-pkgconfigdir=/usr/share/pkgconfig
	
	$(MAKE) -C $(TMPDIR)/$(XF86_INPUT_MOUSE_VERSION)
	$(MAKE) -C $(TMPDIR)/$(XF86_INPUT_MOUSE_VERSION) DESTDIR=$(DISKDIR) pkgconfigdir=/usr/lib/pkgconfig install install