LIB_PACKAGES += build_libxfixes

LIBXFIXES_VERSION = libXfixes-6.0.2
LIBXFIXES_URL = https://www.x.org/releases/individual/lib/$(LIBXFIXES_VERSION).tar.xz

build_libxfixes: build_xorg build_libxt
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXFIXES),y)
	$(MAKE) make_libxfixes
endif

else
	$(MAKE) make_libxfixes
endif
endif

make_libxfixes:
	@echo "Building libXfixes..."
	@if [ ! -f $(TMPDIR)/$(LIBXFIXES_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXFIXES_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXFIXES_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXFIXES_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(LIBXFIXES_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/$(LIBXFIXES_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXFIXES_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install