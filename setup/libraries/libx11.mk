LIB_PACKAGES += build_libx11

LIBX11_VERSION = libX11-1.8.7
LIBX11_URL = https://www.x.org/archive/individual/lib/$(LIBX11_VERSION).tar.xz

build_libx11: build_libxcb
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBX11),y)
	$(MAKE) make_libx11
endif

else
	$(MAKE) make_libx11
endif
endif

make_libx11:
	@echo "Building libX11..."
	@if [ ! -f $(TMPDIR)/$(LIBX11_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBX11_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBX11_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBX11_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBX11_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--disable-specs \
		--enable-ipv6=no \
		--without-xmlto \
		--with-pkgconfigdir=/usr/share/pkgconfig

	$(MAKE) -C $(TMPDIR)/$(LIBX11_VERSION)/src/util makekeys \
		CC="gcc" \
		CFLAGS="-O2" \
		LDFLAGS="" \
		CPPFLAGS=""

	$(MAKE) -C $(TMPDIR)/$(LIBX11_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBX11_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install