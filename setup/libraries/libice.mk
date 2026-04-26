# libICE (Inter-Client Exchange)

LIB_PACKAGES += build_libice

LIBICE_VERSION = libICE-1.1.1
LIBICE_URL = https://www.x.org/releases/individual/lib/$(LIBICE_VERSION).tar.xz

build_libice: build_xorg build_libudev
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBICE),y)
	$(MAKE) make_libice
endif

else
	$(MAKE) make_libice
endif
endif

make_libice:
	@echo "Building libICE..."
	@if [ ! -f $(TMPDIR)/$(LIBICE_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBICE_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBICE_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBICE_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(LIBICE_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/$(LIBICE_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBICE_VERSION) DESTDIR=$(DISKDIR) pkgconfigdir=/usr/lib/pkgconfig install install
	
	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/usr/lib/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/usr/lib/pkgconfig/ 2>/dev/null || true
