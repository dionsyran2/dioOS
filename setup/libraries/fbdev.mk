LIB_PACKAGES += build_fbdev

XF86_VIDEO_FBDEV_GIT = https://gitlab.freedesktop.org/xorg/driver/xf86-video-fbdev.git

build_fbdev: build_xorg
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_FBDEV),y)
	$(MAKE) make_fbdev
endif

else
	$(MAKE) make_fbdev
endif
endif

make_fbdev:
	@if [ ! -d $(TMPDIR)/xf86-video-fbdev ]; then cd $(TMPDIR) && git clone $(XF86_VIDEO_FBDEV_GIT); fi
	cd $(TMPDIR)/xf86-video-fbdev && \
	PKG_CONFIG_DIR="" \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	ACLOCAL="aclocal -I /usr/local/share/aclocal -I $(DISKDIR)/usr/share/aclocal" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib" \
	CFLAGS="-I$(DISKDIR)/usr/include -DXFree86LOADER -DIN_MODULE -DXFree86Server" \
	./autogen.sh --host=$(TARGET_ARCH) --prefix=/usr
	

	$(MAKE) -C $(TMPDIR)/xf86-video-fbdev
	$(MAKE) -C $(TMPDIR)/xf86-video-fbdev pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install