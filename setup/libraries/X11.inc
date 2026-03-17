# X11 has a ton of dependencies... Time to build them

# util-macros
UTIL_MACROS_VERSION = util-macros-1.20.0
UTIL_MACROS_URL = https://www.x.org/archive/individual/util/$(UTIL_MACROS_VERSION).tar.xz

build_util_macros:
	@echo "Building util-macros..."
	@if [ ! -f $(TMPDIR)/$(UTIL_MACROS_VERSION).tar.xz ]; then \
		wget -P $(TMPDIR) $(UTIL_MACROS_URL); \
	fi

	@if [ ! -d $(TMPDIR)/$(UTIL_MACROS_VERSION) ]; then \
		tar -C $(TMPDIR) -xf $(TMPDIR)/$(UTIL_MACROS_VERSION).tar.xz; \
	fi

	cd $(TMPDIR)/$(UTIL_MACROS_VERSION) && \
	./configure \
		--prefix=/usr \
		--host=$(TARGET_ARCH)

	$(MAKE) -C $(TMPDIR)/$(UTIL_MACROS_VERSION)
	$(MAKE) -C $(TMPDIR)/$(UTIL_MACROS_VERSION) DESTDIR=$(DISKDIR) install


# xorgproto
XORGPROTO_VERSION = xorgproto-2024.1
XORGPROTO_URL = https://www.x.org/archive/individual/proto/$(XORGPROTO_VERSION).tar.xz

build_xorgproto:
	@echo "Building xorgproto..."
	@if [ ! -f $(TMPDIR)/$(XORGPROTO_VERSION).tar.xz ]; then \
		wget -P $(TMPDIR) $(XORGPROTO_URL); \
	fi

	@if [ ! -d $(TMPDIR)/$(XORGPROTO_VERSION) ]; then \
		tar -C $(TMPDIR) -xf $(TMPDIR)/$(XORGPROTO_VERSION).tar.xz; \
	fi

	cd $(TMPDIR)/$(XORGPROTO_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig" \
	./configure \
		--prefix=/usr \
		--host=$(TARGET_ARCH)

	$(MAKE) -C $(TMPDIR)/$(XORGPROTO_VERSION)
	$(MAKE) -C $(TMPDIR)/$(XORGPROTO_VERSION) DESTDIR=$(DISKDIR) install

# xtrans
XTRANS_VERSION = xtrans-1.5.0
XTRANS_URL = https://www.x.org/archive/individual/lib/$(XTRANS_VERSION).tar.xz

build_xtrans:
	@echo "Building xtrans..."
	@if [ ! -f $(TMPDIR)/$(XTRANS_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(XTRANS_URL); fi
	@if [ ! -d $(TMPDIR)/$(XTRANS_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XTRANS_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(XTRANS_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig" \
	./configure --prefix=/usr --host=$(TARGET_ARCH)
	$(MAKE) -C $(TMPDIR)/$(XTRANS_VERSION)
	$(MAKE) -C $(TMPDIR)/$(XTRANS_VERSION) DESTDIR=$(DISKDIR) install

# libXau
LIBXAU_VERSION = libXau-1.0.11
LIBXAU_URL = https://www.x.org/archive/individual/lib/$(LIBXAU_VERSION).tar.xz

build_libXau:
	@echo "Building libXau..."
	@if [ ! -f $(TMPDIR)/$(LIBXAU_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXAU_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXAU_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXAU_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(LIBXAU_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH)
	$(MAKE) -C $(TMPDIR)/$(LIBXAU_VERSION)
	$(MAKE) -C $(TMPDIR)/$(LIBXAU_VERSION) DESTDIR=$(DISKDIR) install

# xcb-proto
XCBPROTO_VERSION = xcb-proto-1.16.0
XCBPROTO_URL = https://xorg.freedesktop.org/archive/individual/proto/$(XCBPROTO_VERSION).tar.xz

build_xcbproto:
	@echo "Building xcb-proto..."
	@if [ ! -f $(TMPDIR)/$(XCBPROTO_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(XCBPROTO_URL); fi
	@if [ ! -d $(TMPDIR)/$(XCBPROTO_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XCBPROTO_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(XCBPROTO_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	./configure \
		--prefix=/usr \
		--host=$(TARGET_ARCH)
	$(MAKE) -C $(TMPDIR)/$(XCBPROTO_VERSION)
	$(MAKE) -C $(TMPDIR)/$(XCBPROTO_VERSION) DESTDIR=$(DISKDIR) install

# lib-xcb
LIBXCB_VERSION = libxcb-1.16.1
LIBXCB_URL = https://xorg.freedesktop.org/archive/individual/lib/$(LIBXCB_VERSION).tar.xz

build_libxcb: build_xcbproto
	sed -i 's|/lib/x86_64-dioOS-gnu/||g; s|/usr/lib/x86_64-dioOS-gnu/||g; s|/lib/||g; s|/usr/lib/||g' \
	$(DISKDIR)/usr/lib/libc.so $(DISKDIR)/lib/x86_64-dioOS-gnu/libc.so $(DISKDIR)/usr/lib/libm.so $(DISKDIR)/lib/x86_64-dioOS-gnu/libm.so \
	$(DISKDIR)/usr/lib/libpthread.so $(DISKDIR)/lib/x86_64-dioOS-gnu/libpthread.so 2>/dev/null || true
	@echo "Building libxcb..."
	@if [ ! -f $(TMPDIR)/$(LIBXCB_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXCB_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXCB_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXCB_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBXCB_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--enable-xkb \
		--disable-devel-docs \
		--with-sysroot=$(DISKDIR) \
		XCBPROTO_XCBINCLUDEDIR="$(DISKDIR)/usr/share/xcb"

	cd $(TMPDIR)/$(LIBXCB_VERSION) && \
	PYTHON_PKG_DIR=$$(find $(DISKDIR)/usr/lib -type d -name "*packages" | grep python | head -n 1) && \
	PYTHONPATH="$$PYTHON_PKG_DIR" $(MAKE) -j$(NPROC)
	
	$(MAKE) -C $(TMPDIR)/$(LIBXCB_VERSION) DESTDIR=$(DISKDIR) install


# Time for the state of the art 40 year old library
# libX11

LIBX11_VERSION = libX11-1.8.7
LIBX11_URL = https://www.x.org/archive/individual/lib/$(LIBX11_VERSION).tar.xz

build_libX11: build_libxcb
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
		--without-xmlto

	$(MAKE) -C $(TMPDIR)/$(LIBX11_VERSION)/src/util makekeys \
		CC="gcc" \
		CFLAGS="-O2" \
		LDFLAGS="" \
		CPPFLAGS=""

	$(MAKE) -C $(TMPDIR)/$(LIBX11_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBX11_VERSION) DESTDIR=$(DISKDIR) install

# libXext (Common X Extensions)
LIBXEXT_VERSION = libXext-1.3.6
LIBXEXT_URL = https://www.x.org/archive/individual/lib/$(LIBXEXT_VERSION).tar.xz

build_libXext: build_libX11
	@echo "Building libXext..."
	@if [ ! -f $(TMPDIR)/$(LIBXEXT_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXEXT_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXEXT_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXEXT_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBXEXT_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--disable-specs \
		CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib" \
		CPPFLAGS="-I$(DISKDIR)/usr/include"

	$(MAKE) -C $(TMPDIR)/$(LIBXEXT_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXEXT_VERSION) DESTDIR=$(DISKDIR) install


# libXrender (Anti-aliased Drawing)
LIBXRENDER_VERSION = libXrender-0.9.11
LIBXRENDER_URL = https://www.x.org/archive/individual/lib/$(LIBXRENDER_VERSION).tar.xz

build_libXrender: build_libX11
	@echo "Building libXrender..."
	@if [ ! -f $(TMPDIR)/$(LIBXRENDER_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXRENDER_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXRENDER_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXRENDER_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBXRENDER_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib" \
		CPPFLAGS="-I$(DISKDIR)/usr/include"

	$(MAKE) -C $(TMPDIR)/$(LIBXRENDER_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXRENDER_VERSION) DESTDIR=$(DISKDIR) install

build_x11: build_util_macros build_xorgproto build_xtrans build_libXau build_xcbproto build_libxcb build_libX11 build_libXext build_libXrender
	@echo "X11 Base and XCB Layers complete!"
