LIB_PACKAGES += build_libxdamage

LIBXDAMAGE_VERSION = libXdamage-1.1.7
LIBXDAMAGE_URL = https://www.x.org/releases/individual/lib/$(LIBXDAMAGE_VERSION).tar.xz

build_libxdamage: build_xorg build_libxt
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXDAMAGE),y)
	$(MAKE) make_libxdamage
endif

else
	$(MAKE) make_libxdamage
endif
endif

make_libxdamage:
	@echo "Building libXdamage..."
	@if [ ! -f $(TMPDIR)/$(LIBXDAMAGE_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXDAMAGE_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXDAMAGE_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXDAMAGE_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(LIBXDAMAGE_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/$(LIBXDAMAGE_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXDAMAGE_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install
