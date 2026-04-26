LIB_PACKAGES += build_libxcursor

LIBXCURSOR_VERSION = libXcursor-1.2.3
LIBXCURSOR_URL = https://www.x.org/releases/individual/lib/$(LIBXCURSOR_VERSION).tar.xz

build_libxcursor: build_xorg build_libxt
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXCURSOR),y)
	$(MAKE) make_libxcursor
endif

else
	$(MAKE) make_libxcursor
endif
endif

make_libxcursor:
	@echo "Building libXcursor..."
	@if [ ! -f $(TMPDIR)/$(LIBXCURSOR_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXCURSOR_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXCURSOR_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXCURSOR_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(LIBXCURSOR_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/$(LIBXCURSOR_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXCURSOR_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install