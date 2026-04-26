LIB_PACKAGES += build_libxfont

LIBXFONT2_VERSION = libXfont2-2.0.6
LIBXFONT2_URL = https://www.x.org/releases/individual/lib/$(LIBXFONT2_VERSION).tar.xz

build_libxfont: build_libx11 build_libfontenc build_freetype
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXFONT),y)
	$(MAKE) make_libxfont
endif

else
	$(MAKE) make_libxfont
endif
endif

make_libxfont:
	@echo "Building libXfont2..."
	@if [ ! -f $(TMPDIR)/$(LIBXFONT2_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXFONT2_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXFONT2_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXFONT2_VERSION).tar.xz; fi
	
	sudo find $(DISKDIR) -name "*.la" -delete
	
	cd $(TMPDIR)/$(LIBXFONT2_VERSION) && \
    PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
    PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
    ./configure \
        --host=$(TARGET_ARCH) \
        --prefix=/usr \
        --libdir=/lib/x86_64-dioOS-gnu \
        --with-sysroot=$(DISKDIR) \
        --disable-devel-docs \
        --with-pkgconfigdir=/usr/share/pkgconfig \
        CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
        CFLAGS="-O2 -I$(DISKDIR)/usr/include/X11/fonts" \
        LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu -Wl,-rpath-link,$(DISKDIR)/usr/lib" \
		Z_LIBS="-L$(DISKDIR)/usr/lib -lz" \
		BZ2_LIBS="-L$(DISKDIR)/usr/lib -lbz2"

	$(MAKE) -C $(TMPDIR)/$(LIBXFONT2_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXFONT2_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install