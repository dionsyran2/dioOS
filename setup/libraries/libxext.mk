LIB_PACKAGES += build_libxext

LIBXEXT_VERSION = libXext-1.3.6
LIBXEXT_URL = https://www.x.org/archive/individual/lib/$(LIBXEXT_VERSION).tar.xz

build_libxext: build_libx11
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXEXT),y)
	$(MAKE) make_libxext
endif

else
	$(MAKE) make_libxext
endif
endif

make_libxext:
	@echo "Building libXext..."
	@if [ ! -f $(TMPDIR)/$(LIBXEXT_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXEXT_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXEXT_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXEXT_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBXEXT_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--disable-specs \
		--with-pkgconfigdir=/usr/share/pkgconfig

	$(MAKE) -C $(TMPDIR)/$(LIBXEXT_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXEXT_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install