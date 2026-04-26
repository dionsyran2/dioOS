LIB_PACKAGES += build_pango

PANGO_VERSION = pango-1.28.4
PANGO_TARBALL = $(PANGO_VERSION).tar.bz2
PANGO_URL = https://download.gnome.org/sources/pango/1.28/$(PANGO_TARBALL)


build_pango: build_glib
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_PANGO),y)
	$(MAKE) make_pango
endif

else
	$(MAKE) make_pango
endif
endif

make_pango:
	$(MAKE) build_cairo
	find $(DISKDIR) -name "*.la" -delete

	@echo "Building pango..."  
	@if [ ! -f $(TMPDIR)/$(PANGO_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(PANGO_URL); fi
	@if [ ! -d $(TMPDIR)/$(PANGO_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(PANGO_TARBALL); fi

	find $(DISKDIR) -name "*.la" -delete

	cd $(TMPDIR)/$(PANGO_VERSION) && \
	./configure --prefix=/usr \
		--host=$(TARGET_ARCH) \
		--build=x86_64-pc-linux-gnu \
		--with-sysroot=$(DISKDIR) \
		--disable-introspection \
		--with-x \
		--with-pkgconfigdir=/usr/share/pkgconfig \
		PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
		PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
		CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
		CFLAGS="-O2 -Wno-error=incompatible-pointer-types -DGLIB_DISABLE_DEPRECATION_WARNINGS" \
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu -Wl,-rpath-link,$(DISKDIR)/usr/lib"

	$(MAKE) -C $(TMPDIR)/$(PANGO_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(PANGO_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install