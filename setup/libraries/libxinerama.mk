LIB_PACKAGES += build_libxinerama

LIBXINERAMA_VERSION = 1.1.5
LIBXINERAMA_URL = https://www.x.org/releases/individual/lib/libXinerama-$(LIBXINERAMA_VERSION).tar.xz

build_libxinerama: build_xorg build_libexpat
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXINERAMA),y)
	$(MAKE) make_libxinerama
endif

else
	$(MAKE) make_libxinerama
endif
endif

make_libxinerama:
	@echo "Building libXinerama..."
	@if [ ! -f $(TMPDIR)/libXinerama-$(LIBXINERAMA_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXINERAMA_URL); fi
	@if [ ! -d $(TMPDIR)/libXinerama-$(LIBXINERAMA_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/libXinerama-$(LIBXINERAMA_VERSION).tar.xz; fi
	cd $(TMPDIR)/libXinerama-$(LIBXINERAMA_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/libXinerama-$(LIBXINERAMA_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/libXinerama-$(LIBXINERAMA_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install