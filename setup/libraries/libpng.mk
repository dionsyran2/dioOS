LIB_PACKAGES += build_libpng

LIBPNG_VERSION = 1.6.40
LIBPNG_TARBALL = libpng-$(LIBPNG_VERSION).tar.xz
LIBPNG_URL = https://download.sourceforge.net/libpng/$(LIBPNG_TARBALL)

build_libpng:
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)
ifeq ($(BUILD_LIBPNG),y)
	$(MAKE) make_libpng
endif
else
	$(MAKE) make_libpng
endif
endif

make_libpng:
	@echo "Building libpng..."
	@if [ ! -f $(TMPDIR)/$(LIBPNG_TARBALL) ]; then wget -P $(TMPDIR) $(LIBPNG_URL); fi
	@if [ ! -d $(TMPDIR)/libpng-$(LIBPNG_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBPNG_TARBALL); fi
	
	cd $(TMPDIR)/libpng-$(LIBPNG_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--with-pkgconfigdir=/usr/share/pkgconfig

	$(MAKE) -C $(TMPDIR)/libpng-$(LIBPNG_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/libpng-$(LIBPNG_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install
	
	@ln -sf libpng16 $(DISKDIR)/usr/include/libpng
	@ln -sf libpng16/png.h $(DISKDIR)/usr/include/png.h
	@ln -sf libpng16/pngconf.h $(DISKDIR)/usr/include/pngconf.h
	@ln -sf libpng16/pnglibconf.h $(DISKDIR)/usr/include/pnglibconf.h