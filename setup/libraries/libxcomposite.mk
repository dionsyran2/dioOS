LIB_PACKAGES += build_libxcomposite

LIBXCOMPOSITE_VERSION = libXcomposite-0.4.7
LIBXCOMPOSITE_URL = https://www.x.org/releases/individual/lib/$(LIBXCOMPOSITE_VERSION).tar.xz

build_libxcomposite: build_xorg build_libxt
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXCOMPOSITE),y)
	$(MAKE) make_libxcomposite
endif

else
	$(MAKE) make_libxcomposite
endif
endif

make_libxcomposite:
	@echo "Building libXcomposite..."
	@if [ ! -f $(TMPDIR)/$(LIBXCOMPOSITE_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXCOMPOSITE_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXCOMPOSITE_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXCOMPOSITE_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(LIBXCOMPOSITE_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/$(LIBXCOMPOSITE_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXCOMPOSITE_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install