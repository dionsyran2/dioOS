# libXpm (X Pixmap Library)

LIB_PACKAGES += build_libxpm

LIBXPM_VERSION = libXpm-3.5.17
LIBXPM_URL = https://www.x.org/releases/individual/lib/$(LIBXPM_VERSION).tar.xz

build_libxpm: build_xorg build_libxt
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXPM),y)
	$(MAKE) make_libxpm
endif

else
	$(MAKE) make_libxpm
endif
endif

make_libxpm:
	@echo "Building libXpm..."
	@if [ ! -f $(TMPDIR)/$(LIBXPM_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXPM_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXPM_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXPM_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(LIBXPM_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --disable-open-zfile --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/$(LIBXPM_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXPM_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install