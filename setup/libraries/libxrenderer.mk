LIB_PACKAGES += build_libxrenderer

LIBXRENDER_VERSION = libXrender-0.9.11
LIBXRENDER_URL = https://www.x.org/archive/individual/lib/$(LIBXRENDER_VERSION).tar.xz

build_libxrenderer: build_libx11
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXRENDERER),y)
	$(MAKE) make_libxrenderer
endif

else
	$(MAKE) make_libxrenderer
endif
endif

make_libxrenderer:
	@echo "Building libXrender..."
	@if [ ! -f $(TMPDIR)/$(LIBXRENDER_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXRENDER_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXRENDER_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXRENDER_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBXRENDER_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--with-pkgconfigdir=/usr/share/pkgconfig

	$(MAKE) -C $(TMPDIR)/$(LIBXRENDER_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXRENDER_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install