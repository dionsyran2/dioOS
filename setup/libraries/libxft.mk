LIB_PACKAGES += build_libxft

LIBXFT_VERSION = 2.3.8
LIBXFT_URL = https://www.x.org/releases/individual/lib/libXft-$(LIBXFT_VERSION).tar.xz

build_libxft: build_xorg build_libexpat
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXFT),y)
	$(MAKE) make_libxft
endif

else
	$(MAKE) make_libxft
endif
endif

make_libxft:
	@echo "Building libXft..."
	@if [ ! -f $(TMPDIR)/libXft-$(LIBXFT_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXFT_URL); fi
	@if [ ! -d $(TMPDIR)/libXft-$(LIBXFT_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/libXft-$(LIBXFT_VERSION).tar.xz; fi
	cd $(TMPDIR)/libXft-$(LIBXFT_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/libXft-$(LIBXFT_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/libXft-$(LIBXFT_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install
