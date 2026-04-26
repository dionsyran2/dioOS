LIB_PACKAGES += build_libxrandr


LIBXRANDR_VERSION = 1.5.4
LIBXRANDR_URL = https://www.x.org/releases/individual/lib/libXrandr-$(LIBXRANDR_VERSION).tar.xz

build_libxrandr: build_xorg build_libexpat
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXRANDR),y)
	$(MAKE) make_libxrandr
endif

else
	$(MAKE) make_libxrandr
endif
endif

make_libxrandr:
	@echo "Building libXrandr..."
	@if [ ! -f $(TMPDIR)/libXrandr-$(LIBXRANDR_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXRANDR_URL); fi
	@if [ ! -d $(TMPDIR)/libXrandr-$(LIBXRANDR_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/libXrandr-$(LIBXRANDR_VERSION).tar.xz; fi
	cd $(TMPDIR)/libXrandr-$(LIBXRANDR_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/libXrandr-$(LIBXRANDR_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/libXrandr-$(LIBXRANDR_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install