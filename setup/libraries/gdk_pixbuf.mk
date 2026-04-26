LIB_PACKAGES += build_gdk_pixbuf

GDK_PIXBUF_VERSION = gdk-pixbuf-2.26.5
GDK_PIXBUF_TARBALL = $(GDK_PIXBUF_VERSION).tar.xz
GDK_PIXBUF_URL = https://download.gnome.org/sources/gdk-pixbuf/2.26/$(GDK_PIXBUF_TARBALL)

build_gdk_pixbuf: build_glib build_libpng build_libtiff
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_GDK_PIXBUF),y)
	$(MAKE) make_gdk_pixbuf
endif

else
	$(MAKE) make_gdk_pixbuf
endif
endif

make_gdk_pixbuf: 
	@echo "Building GdkPixbuf..."
	@if [ ! -f $(TMPDIR)/$(GDK_PIXBUF_TARBALL) ]; then wget -P $(TMPDIR) $(GDK_PIXBUF_URL); fi
	@if [ ! -d $(TMPDIR)/$(GDK_PIXBUF_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(GDK_PIXBUF_TARBALL); fi

	find $(DISKDIR) -name "*.la" -delete

	cd $(TMPDIR)/$(GDK_PIXBUF_VERSION) && \
	export PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" && \
	export PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" && \
	./configure --prefix=/usr \
		--host=$(TARGET_ARCH) \
		--build=x86_64-pc-linux-gnu \
		--with-sysroot=$(DISKDIR) \
		--disable-introspection \
		--with-libpng \
		--with-pkgconfigdir=/usr/share/pkgconfig \
		CPPFLAGS="-I$(DISKDIR)/usr/include" \
		LDFLAGS="-L$(DISKDIR)/usr/lib -L$(DISKDIR)/lib/x86_64-dioOS-gnu -Wl,-rpath-link=$(DISKDIR)/usr/lib -Wl,-rpath-link=$(DISKDIR)/lib/x86_64-dioOS-gnu" \
		LIBS="-lpng16 -ljpeg -ltiff -lz -lm" \
		gio_can_sniff=yes && \
	$(MAKE) -j$(NPROC) && \
	$(MAKE) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install