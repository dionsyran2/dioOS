LIB_PACKAGES += build_libfontenc

LIBFONTENC_VERSION = libfontenc-1.1.8
LIBFONTENC_URL = https://www.x.org/releases/individual/lib/$(LIBFONTENC_VERSION).tar.xz

build_libfontenc: build_glibc
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBFONTENC),y)
	$(MAKE) make_libfontenc
endif

else
	$(MAKE) make_libfontenc
endif
endif
make_libfontenc:
	@echo "Building libfontenc..."
	@if [ ! -f $(TMPDIR)/$(LIBFONTENC_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBFONTENC_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBFONTENC_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBFONTENC_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBFONTENC_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--with-pkgconfigdir=/usr/share/pkgconfig \
		--with-png=yes \
		--with-zlib=yes \
		--with-brotli=no \
		--with-harfbuzz=no


	$(MAKE) -C $(TMPDIR)/$(LIBFONTENC_VERSION) -j$(NPROC) \

	$(MAKE) -C $(TMPDIR)/$(LIBFONTENC_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install