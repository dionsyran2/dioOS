LIB_PACKAGES += build_gdk2

GDK2+_VERSION = gtk+-2.24.33
GDK2+_TARBALL = $(GDK2+_VERSION).tar.xz
GDK2+_URL = https://download.gnome.org/sources/gtk+/2.24/$(GDK2+_TARBALL)


build_gdk2: build_xorg
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_GDK2),y)
	$(MAKE) make_gdk2
endif

else
	$(MAKE) make_gdk2
endif
endif

make_gdk2: build_gdk_pixbuf build_atk
	@echo "Building gdk+2 ..."
	@if [ ! -f $(TMPDIR)/$(GDK2+_TARBALL) ]; then wget -P $(TMPDIR) $(GDK2+_URL); fi
	@if [ ! -d $(TMPDIR)/$(GDK2+_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(GDK2+_TARBALL); fi
	
	find $(DISKDIR) -name "*.la" -delete

	cd $(TMPDIR)/$(GDK2+_VERSION) && \
	./configure --prefix=/usr \
		--host=$(TARGET_ARCH) \
		--build=x86_64-pc-linux-gnu \
		--with-sysroot=$(DISKDIR) \
		--disable-fribidi \
		--disable-introspection \
		--with-gdktarget=x11 \
		PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
		PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
		CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
		CFLAGS="-O2 -fpermissive -Wno-error=implicit-int -Wno-error=incompatible-pointer-types -DGLIB_DISABLE_DEPRECATION_WARNINGS" \
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu -Wl,-rpath-link,$(DISKDIR)/usr/lib"

	$(MAKE) -C $(TMPDIR)/$(GDK2+_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(GDK2+_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install