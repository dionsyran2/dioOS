PACKAGES += build_xorg

XORG_SERVER_VERSION = xorg-server-21.1.11
XORG_SERVER_URL = https://www.x.org/archive/individual/xserver/$(XORG_SERVER_VERSION).tar.xz


build_xorg: build_libxau build_libx11 build_pixman build_libxfont build_libxdmcp build_libxcb build_zlib build_openssl build_libxcvt build_libpciaccess
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_XORG),y)
	$(MAKE) make_xorg
endif

else
	$(MAKE) make_xorg
endif
endif

build_macros:
	@if [ ! -d $(TMPDIR)/macros ]; then cd $(TMPDIR) && git clone https://gitlab.freedesktop.org/xorg/util/macros.git; fi
	cd $(TMPDIR)/macros && \
	./autogen.sh --prefix=/usr/local && \
	$(MAKE) && \
	$(MAKE) DESTDIR=$(DISKDIR) install

make_xorg: 
	@echo "Building xorg-server..."
	@if [ ! -f $(TMPDIR)/$(XORG_SERVER_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(XORG_SERVER_URL); fi
	@rm -rf $(TMPDIR)/$(XORG_SERVER_VERSION)
	@tar -C $(TMPDIR) -xf $(TMPDIR)/$(XORG_SERVER_VERSION).tar.xz
	
	cd $(TMPDIR)/$(XORG_SERVER_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--enable-xorg \
		--disable-glx \
		--disable-glamor \
		--disable-dri \
		--disable-dri2 \
		--disable-dri3 \
		--disable-udev \
		--disable-config-udev \
		--disable-systemd-logind \
		--disable-xwayland \
		--disable-libdrm \
		--disable-xshmfence \
		--with-sha1=libcrypto \
		--with-fontrootdir=/usr/share/fonts/X11 \
		CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib \
		-L$(DISKDIR)/usr/lib/x86_64-dioOS-gnu -Wl,-rpath-link=$(DISKDIR)/lib/x86_64-dioOS-gnu \
		-Wl,-rpath-link=$(DISKDIR)/usr/lib/x86_64-dioOS-gnu" \
		CPPFLAGS="-I$(DISKDIR)/usr/include" \
		CFLAGS="-I$(DISKDIR)/usr/include" \
		LIBS="-lpixman-1 -lXfont2 -lXdmcp -lz -lcrypto"

	$(MAKE) -C $(TMPDIR)/$(XORG_SERVER_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(XORG_SERVER_VERSION) DESTDIR=$(DISKDIR) install

	$(MAKE) build_macros

	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/usr/lib/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/usr/lib/pkgconfig/ 2>/dev/null || true