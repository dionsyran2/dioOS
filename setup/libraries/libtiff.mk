LIB_PACKAGES += build_libtiff

LIBTIFF_VERSION = 4.6.0
LIBTIFF_URL = https://download.osgeo.org/libtiff/tiff-$(LIBTIFF_VERSION).tar.gz

build_libtiff: build_glibc build_zlib build_libjpeg
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBTIFF),y)
	$(MAKE) make_libtiff
endif

else
	$(MAKE) make_libtiff
endif

make_libtiff:
	@echo "Building libtiff..."
	@if [ ! -f $(TMPDIR)/tiff-$(LIBTIFF_VERSION).tar.gz ]; then wget -P $(TMPDIR) $(LIBTIFF_URL); fi
	@if [ ! -d $(TMPDIR)/tiff-$(LIBTIFF_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/tiff-$(LIBTIFF_VERSION).tar.gz; fi
	
	cd $(TMPDIR)/tiff-$(LIBTIFF_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--with-pkgconfigdir=/usr/share/pkgconfig \
		--disable-webp \
		--disable-zstd

	$(MAKE) -C $(TMPDIR)/tiff-$(LIBTIFF_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/tiff-$(LIBTIFF_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install