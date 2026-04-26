LIB_PACKAGES += build_libxcb

LIBXCB_VERSION = libxcb-1.16.1
LIBXCB_URL = https://xorg.freedesktop.org/archive/individual/lib/$(LIBXCB_VERSION).tar.xz


build_libxcb: build_glibc build_util_macros build_xcbproto
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXCB),y)
	$(MAKE) make_libxcb
endif

else
	$(MAKE) make_libxcb
endif
endif

make_libxcb:
	@sed -i 's|/lib/x86_64-dioOS-gnu/||g; s|/usr/lib/x86_64-dioOS-gnu/||g; s|/lib/||g; s|/usr/lib/||g' \
	$(DISKDIR)/usr/lib/libc.so $(DISKDIR)/lib/x86_64-dioOS-gnu/libc.so $(DISKDIR)/usr/lib/libm.so $(DISKDIR)/lib/x86_64-dioOS-gnu/libm.so \
	$(DISKDIR)/usr/lib/libpthread.so $(DISKDIR)/lib/x86_64-dioOS-gnu/libpthread.so 2>/dev/null || true
	@echo "Building libxcb..."
	@if [ ! -f $(TMPDIR)/$(LIBXCB_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXCB_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXCB_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXCB_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBXCB_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--enable-xkb \
		--disable-devel-docs \
		--with-sysroot=$(DISKDIR) \
		XCBPROTO_XCBINCLUDEDIR="$(DISKDIR)/usr/share/xcb" \
		--with-pkgconfigdir=/usr/share/pkgconfig

	cd $(TMPDIR)/$(LIBXCB_VERSION) && \
	PYTHON_PKG_DIR=$$(find $(DISKDIR)/usr/lib -type d -name "*packages" | grep python | head -n 1) && \
	PYTHONPATH="$$PYTHON_PKG_DIR" $(MAKE) -j$(NPROC)
	
	$(MAKE) -C $(TMPDIR)/$(LIBXCB_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install