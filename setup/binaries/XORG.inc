# pixman (The Pixel Math Engine)
PIXMAN_VERSION = pixman-0.42.2
PIXMAN_URL = https://xorg.freedesktop.org/archive/individual/lib/$(PIXMAN_VERSION).tar.gz

build_pixman: build_libXrender
	@echo "Building pixman..."
	@if [ ! -f $(TMPDIR)/$(PIXMAN_VERSION).tar.gz ]; then wget -P $(TMPDIR) $(PIXMAN_URL); fi
	@if [ ! -d $(TMPDIR)/$(PIXMAN_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(PIXMAN_VERSION).tar.gz; fi
	
	cd $(TMPDIR)/$(PIXMAN_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--disable-gtk \
		--disable-libpng

	$(MAKE) -C $(TMPDIR)/$(PIXMAN_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(PIXMAN_VERSION) DESTDIR=$(DISKDIR) install

# libxkbfile (Keyboard Layout Parser)
LIBXKBFILE_VERSION = libxkbfile-1.1.2
LIBXKBFILE_URL = https://www.x.org/archive/individual/lib/$(LIBXKBFILE_VERSION).tar.xz

build_libxkbfile: build_pixman
	@echo "Building libxkbfile..."
	@if [ ! -f $(TMPDIR)/$(LIBXKBFILE_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXKBFILE_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXKBFILE_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXKBFILE_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBXKBFILE_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) 
    
	$(MAKE) -C $(TMPDIR)/$(LIBXKBFILE_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXKBFILE_VERSION) DESTDIR=$(DISKDIR) install

# xorg-server (The Core Display Server)
XORG_SERVER_VERSION = xorg-server-21.1.11
XORG_SERVER_URL = https://www.x.org/archive/individual/xserver/$(XORG_SERVER_VERSION).tar.xz


# libxcvt
LIBXCVT_VERSION = libxcvt-0.1.2
LIBXCVT_URL = https://www.x.org/releases/individual/lib/$(LIBXCVT_VERSION).tar.xz

build_libxcvt:
	@echo "Building libxcvt (Meson)..."
	@if [ ! -f $(TMPDIR)/$(LIBXCVT_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXCVT_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXCVT_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXCVT_VERSION).tar.xz; fi
	
	@echo "[binaries]" > $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "c = '$(CROSS_GCC)'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "ar = '$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ar'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "strip = '$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-strip'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "pkgconfig = 'pkg-config'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "[built-in options]" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "c_args = ['--sysroot=$(DISKDIR)', '-I$(DISKDIR)/usr/include']" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "c_link_args = ['--sysroot=$(DISKDIR)', '-L$(DISKDIR)/lib/x86_64-dioOS-gnu', '-L$(DISKDIR)/lib', '-L$(DISKDIR)/usr/lib']" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "[properties]" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "sys_root = '$(DISKDIR)'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "pkg_config_libdir = ['$(DISKDIR)/usr/lib/pkgconfig', '$(DISKDIR)/usr/share/pkgconfig', '$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig']" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "[host_machine]" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "system = 'linux'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "cpu_family = 'x86_64'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "cpu = 'x86_64'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "endian = 'little'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt

	cd $(TMPDIR)/$(LIBXCVT_VERSION) && \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	meson setup build --cross-file cross_file.txt --prefix=/usr --libdir=lib/x86_64-dioOS-gnu
	
	ninja -C $(TMPDIR)/$(LIBXCVT_VERSION)/build
	DESTDIR=$(DISKDIR) ninja -C $(TMPDIR)/$(LIBXCVT_VERSION)/build install


# freetype (Font Rasterization)
FREETYPE_VERSION = freetype-2.13.2
FREETYPE_URL = https://download.savannah.gnu.org/releases/freetype/$(FREETYPE_VERSION).tar.xz

build_freetype:
	@echo "Building freetype..."
	@if [ ! -f $(TMPDIR)/$(FREETYPE_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(FREETYPE_URL); fi
	@if [ ! -d $(TMPDIR)/$(FREETYPE_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(FREETYPE_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(FREETYPE_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--without-png \
		--without-harfbuzz \
		--without-bzip2

	$(MAKE) -C $(TMPDIR)/$(FREETYPE_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(FREETYPE_VERSION) DESTDIR=$(DISKDIR) install


# libfontenc (Font Encodings)
LIBFONTENC_VERSION = libfontenc-1.1.8
LIBFONTENC_URL = https://www.x.org/releases/individual/lib/$(LIBFONTENC_VERSION).tar.xz

build_libfontenc:
	@echo "Building libfontenc..."
	@if [ ! -f $(TMPDIR)/$(LIBFONTENC_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBFONTENC_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBFONTENC_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBFONTENC_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBFONTENC_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) 

	$(MAKE) -C $(TMPDIR)/$(LIBFONTENC_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBFONTENC_VERSION) DESTDIR=$(DISKDIR) install


# libXfont2
LIBXFONT2_VERSION = libXfont2-2.0.6
LIBXFONT2_URL = https://www.x.org/releases/individual/lib/$(LIBXFONT2_VERSION).tar.xz

build_libXfont2: build_freetype build_libfontenc
	@echo "Building libXfont2..."
	@if [ ! -f $(TMPDIR)/$(LIBXFONT2_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXFONT2_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXFONT2_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXFONT2_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBXFONT2_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	CFLAGS="-O2 -I$(DISKDIR)/usr/include -Wno-error=implicit-function-declaration" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib" \
	CPPFLAGS="-I$(DISKDIR)/usr/include" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--disable-devel-docs

	$(MAKE) -C $(TMPDIR)/$(LIBXFONT2_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXFONT2_VERSION) DESTDIR=$(DISKDIR) install

LIBXDMCP_VERSION = libXdmcp-1.1.4
LIBXDMCP_URL = https://www.x.org/archive/individual/lib/$(LIBXDMCP_VERSION).tar.xz

build_libXdmcp:
	@echo "Building libXdmcp..."
	@if [ ! -f $(TMPDIR)/$(LIBXDMCP_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXDMCP_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXDMCP_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXDMCP_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBXDMCP_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig:$(DISKDIR)/usr/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--disable-docs \
		CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib" \
		CPPFLAGS="-I$(DISKDIR)/usr/include"

	$(MAKE) -C $(TMPDIR)/$(LIBXDMCP_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXDMCP_VERSION) DESTDIR=$(DISKDIR) install

# zlib (The Compression Standard)
ZLIB_VERSION = zlib-1.3.2
ZLIB_URL = https://zlib.net/$(ZLIB_VERSION).tar.gz
build_zlib:
	@echo "Building zlib..."
	@if [ ! -f $(TMPDIR)/$(ZLIB_VERSION).tar.gz ]; then wget -P $(TMPDIR) $(ZLIB_URL); fi
	@if [ ! -d $(TMPDIR)/zlib ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(ZLIB_VERSION).tar.gz; fi
	
	cd $(TMPDIR)/$(ZLIB_VERSION) && \
	CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
	AR="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ar" \
	RANLIB="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ranlib" \
	CFLAGS="-O2 -I$(DISKDIR)/usr/include" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu

	$(MAKE) -C $(TMPDIR)/$(ZLIB_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(ZLIB_VERSION) DESTDIR=$(DISKDIR) install

OPENSSL_VERSION = openssl-3.2.1
OPENSSL_URL = https://www.openssl.org/source/$(OPENSSL_VERSION).tar.gz

build_openssl:
	@echo "Building OpenSSL..."
	@if [ ! -f $(TMPDIR)/$(OPENSSL_VERSION).tar.gz ]; then wget -P $(TMPDIR) $(OPENSSL_URL); fi
	@if [ ! -d $(TMPDIR)/$(OPENSSL_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(OPENSSL_VERSION).tar.gz; fi
	
	cd $(TMPDIR)/$(OPENSSL_VERSION) && \
	./Configure linux-x86_64 \
		--prefix=/usr \
		--libdir=lib/x86_64-dioOS-gnu \
		--cross-compile-prefix=$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)- \
		no-async no-shared no-tests no-unit-test no-threads \
		--sysroot=$(DISKDIR) \
		-I$(DISKDIR)/usr/include \
		-L$(DISKDIR)/lib/x86_64-dioOS-gnu \
		-D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64

	$(MAKE) -C $(TMPDIR)/$(OPENSSL_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(OPENSSL_VERSION) DESTDIR=$(DISKDIR) install_sw

build_xorg_server:
	@echo "Building xorg-server..."
	@if [ ! -f $(TMPDIR)/$(XORG_SERVER_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(XORG_SERVER_URL); fi
	@rm -rf $(TMPDIR)/$(XORG_SERVER_VERSION)
	@tar -C $(TMPDIR) -xf $(TMPDIR)/$(XORG_SERVER_VERSION).tar.xz
	
	cd $(TMPDIR)/$(XORG_SERVER_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig:$(DISKDIR)/usr/lib/x86_64-dioOS-gnu/pkgconfig" \
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
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link=$(DISKDIR)/lib/x86_64-dioOS-gnu" \
		CPPFLAGS="-I$(DISKDIR)/usr/include" \
		CFLAGS="-I$(DISKDIR)/usr/include" \
		LIBS="-lpixman-1 -lXfont2 -lXdmcp -lz -lcrypto"

	$(MAKE) -C $(TMPDIR)/$(XORG_SERVER_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(XORG_SERVER_VERSION) DESTDIR=$(DISKDIR) install

# xinit
XINIT_VERSION = xinit-1.4.2
XINIT_URL = https://www.x.org/releases/individual/app/$(XINIT_VERSION).tar.gz

build_xinit:
	@echo "Building xinit..."
	@if [ ! -f $(TMPDIR)/$(XINIT_VERSION).tar.gz ]; then wget -P $(TMPDIR) $(XINIT_URL); fi
	@if [ ! -d $(TMPDIR)/$(XINIT_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XINIT_VERSION).tar.gz; fi
	
	cd $(TMPDIR)/$(XINIT_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig:$(DISKDIR)/usr/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--with-xinitdir=/etc/X11/xinit \
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib" \
		CFLAGS="-I$(DISKDIR)/usr/include"

	$(MAKE) -C $(TMPDIR)/$(XINIT_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(XINIT_VERSION) DESTDIR=$(DISKDIR) install

build_macros:
	@if [ ! -d $(TMPDIR)/macros ]; then cd $(TMPDIR) && git clone https://gitlab.freedesktop.org/xorg/util/macros.git; fi
	cd $(TMPDIR)/macros && \
	./autogen.sh --prefix=/usr/local && \
	$(MAKE) && \
	$(MAKE) install

# xf86-video-fbdev (fb driver)
XF86_VIDEO_FBDEV_GIT = https://gitlab.freedesktop.org/xorg/driver/xf86-video-fbdev.git
build_xf86_video_fbdev: build_macros
	@if [ ! -d $(TMPDIR)/xf86-video-fbdev ]; then cd $(TMPDIR) && git clone $(XF86_VIDEO_FBDEV_GIT); fi
	cd $(TMPDIR)/xf86-video-fbdev && \
	PKG_CONFIG_DIR="" \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig:$(DISKDIR)/usr/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	ACLOCAL="aclocal -I /usr/local/share/aclocal -I $(DISKDIR)/usr/share/aclocal" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib" \
	CFLAGS="-I$(DISKDIR)/usr/include -DXFree86LOADER -DIN_MODULE -DXFree86Server" \
	./autogen.sh --host=$(TARGET_ARCH) --prefix=/usr
	

	$(MAKE) -C $(TMPDIR)/xf86-video-fbdev
	$(MAKE) -C $(TMPDIR)/xf86-video-fbdev DESTDIR=$(DISKDIR) install


# keyboard
XF86_INPUT_EVDEV_VERSION = xf86-input-evdev-2.7.3
XF86_INPUT_EVDEV_URL = https://www.x.org/releases/individual/driver/$(XF86_INPUT_EVDEV_VERSION).tar.bz2

build_xf86_input_evdev: build_macros
	@echo "Building vintage evdev (no libevdev/mtdev required!)..."
	@if [ ! -f $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION).tar.bz2 ]; then wget -P $(TMPDIR) $(XF86_INPUT_EVDEV_URL); fi
	@if [ ! -d $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION).tar.bz2; fi
	
	cd $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION) && \
	PKG_CONFIG_DIR="" \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig:$(DISKDIR)/usr/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link=$(DISKDIR)/lib/x86_64-dioOS-gnu" \
	CPPFLAGS="-I$(DISKDIR)/usr/include" \
	CFLAGS="-I$(DISKDIR)/usr/include" \
	./configure --host=$(TARGET_ARCH) --prefix=/usr

	SYSROOT=$(DISKDIR) $(MAKE) -C $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION)
	$(MAKE) -C $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION) DESTDIR=$(DISKDIR) install
	
# --- Whatever this is ---
XKB_CONFIG_VERSION = xkeyboard-config-2.38
XKB_CONFIG_URL = https://www.x.org/releases/individual/data/xkeyboard-config/$(XKB_CONFIG_VERSION).tar.xz
build_xkeyboard_config:
	@echo "Installing XKB keyboard layouts..."
	@if [ ! -f $(TMPDIR)/$(XKB_CONFIG_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(XKB_CONFIG_URL); fi
	@if [ ! -d $(TMPDIR)/$(XKB_CONFIG_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XKB_CONFIG_VERSION).tar.xz; fi
	
	# Since this is mostly just text files, we can use standard meson without a strict cross-file
	cd $(TMPDIR)/$(XKB_CONFIG_VERSION) && \
	meson setup build --prefix=/usr
	
	ninja -C $(TMPDIR)/$(XKB_CONFIG_VERSION)/build
	DESTDIR=$(DISKDIR) ninja -C $(TMPDIR)/$(XKB_CONFIG_VERSION)/build install

LIBUDEV_ZERO_GIT = https://github.com/illiliti/libudev-zero.git

build_libudev:
	@echo "Building libudev-zero..."
	@if [ ! -d $(TMPDIR)/libudev-zero ]; then cd $(TMPDIR) && git clone $(LIBUDEV_ZERO_GIT); fi
	
	$(MAKE) -C $(TMPDIR)/libudev-zero \
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link=$(DISKDIR)/lib/x86_64-dioOS-gnu" \
		CPPFLAGS="-I$(DISKDIR)/usr/include" \
		CFLAGS="-I$(DISKDIR)/usr/include" \
		AR="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ar" \
		PREFIX=/usr \
		all
		
	$(MAKE) -C $(TMPDIR)/libudev-zero \
		PREFIX=/usr \
		DESTDIR=$(DISKDIR) \
		install

# xkbcomp (The actual keyboard compiler binary!)
XKBCOMP_VERSION = xkbcomp-1.4.6
XKBCOMP_URL = https://www.x.org/releases/individual/app/$(XKBCOMP_VERSION).tar.xz

build_xkbcomp: build_libxkbfile
	@echo "Building xkbcomp..."
	@if [ ! -f $(TMPDIR)/$(XKBCOMP_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(XKBCOMP_URL); fi
	@if [ ! -d $(TMPDIR)/$(XKBCOMP_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XKBCOMP_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(XKBCOMP_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link=$(DISKDIR)/lib/x86_64-dioOS-gnu" \
	CPPFLAGS="-I$(DISKDIR)/usr/include" \
	CFLAGS="-I$(DISKDIR)/usr/include" \
	AR="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ar" \
	PREFIX=/usr \
	./configure \
		--prefix=/usr \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--with-xkb-config-root=/usr/share/X11/xkb
	
	$(MAKE) -C $(TMPDIR)/$(XKBCOMP_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(XKBCOMP_VERSION) DESTDIR=$(DISKDIR) install


build_xorg: build_xinit build_openssl build_pixman build_libxkbfile build_libxcvt build_freetype build_libfontenc build_libXdmcp build_libXfont2 build_zlib build_xorg_server build_macros \
	build_xf86_video_fbdev build_libudev build_xf86_input_evdev build_xkbcomp build_xkeyboard_config