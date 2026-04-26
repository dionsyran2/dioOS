LIB_PACKAGES += build_cairo

CAIRO_GIT = https://github.com/msteinert/cairo.git
CAIRO_PATH := $(TMPDIR)/cairo

build_cairo: build_glib
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_CAIRO),y)
	$(MAKE) make_cairo
endif

else
	$(MAKE) make_cairo
endif
endif

make_cairo: 
	@if [ ! -d $(CAIRO_PATH) ]; then \
		git clone $(CAIRO_GIT) $(CAIRO_PATH); \
	fi

	cd $(CAIRO_PATH) && \
	NOCONFIGURE=1 PATH=".:$(PATH)" ./autogen.sh && \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	./configure --prefix=/usr \
	            --host=$(TARGET_ARCH) \
	            --with-sysroot=$(DISKDIR) \
	            --enable-xlib \
	            --enable-ft \
	            --enable-fc \
	            --enable-png \
	            --disable-gtk-doc \
				--with-pkgconfigdir=/usr/share/pkgconfig \
		CAIRO_CFLAGS="-I$(DISKDIR)/usr/include/cairo -I$(DISKDIR)/usr/include/freetype2" \
		CAIRO_LIBS="-L$(DISKDIR)/usr/lib -lcairo -lfreetype -lfontconfig" && \
	$(MAKE) -C $(CAIRO_PATH) -j$(NPROC)
	$(MAKE) -C $(CAIRO_PATH) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install