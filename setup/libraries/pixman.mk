LIB_PACKAGES += build_pixman

PIXMAN_VERSION = pixman-0.42.2
PIXMAN_URL = https://xorg.freedesktop.org/archive/individual/lib/$(PIXMAN_VERSION).tar.gz

build_pixman: build_libx11
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_PIXMAN),y)
	$(MAKE) make_pixman
endif

else
	$(MAKE) make_pixman
endif
endif

make_pixman:
	@echo "Building pixman..."
	@if [ ! -f $(TMPDIR)/$(PIXMAN_VERSION).tar.gz ]; then wget -P $(TMPDIR) $(PIXMAN_URL); fi
	@if [ ! -d $(TMPDIR)/$(PIXMAN_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(PIXMAN_VERSION).tar.gz; fi
	
	cd $(TMPDIR)/$(PIXMAN_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--disable-gtk \
		--disable-libpng \
		--with-pkgconfigdir=/usr/share/pkgconfig

	$(MAKE) -C $(TMPDIR)/$(PIXMAN_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(PIXMAN_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install