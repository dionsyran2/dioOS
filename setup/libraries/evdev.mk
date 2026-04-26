LIB_PACKAGES += build_evdev

XF86_INPUT_EVDEV_VERSION = xf86-input-evdev-2.8.4
XF86_INPUT_EVDEV_URL = https://www.x.org/releases/individual/driver/$(XF86_INPUT_EVDEV_VERSION).tar.bz2

build_evdev: build_xorg build_libudev
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_EVDEV),y)
	$(MAKE) make_evdev
endif

else
	$(MAKE) make_evdev
endif
endif

make_evdev:
	@echo "Building vintage evdev (no libevdev/mtdev required!)..."
	@if [ ! -f $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION).tar.bz2 ]; then wget -P $(TMPDIR) $(XF86_INPUT_EVDEV_URL); fi
	@if [ ! -d $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION).tar.bz2; fi
	
	cd $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION) && \
	PKG_CONFIG_DIR="" \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link=$(DISKDIR)/lib/x86_64-dioOS-gnu" \
	CPPFLAGS="-I$(DISKDIR)/usr/include" \
	CFLAGS="-I$(DISKDIR)/usr/include" \
	./configure --host=$(TARGET_ARCH) --prefix=/usr --disable-libudev --disable-mtdev --disable-libevdev --with-pkgconfigdir=/usr/share/pkgconfig

	sed -i 's/#define HAVE_LIBUDEV 1/\/* #undef HAVE_LIBUDEV *\//g' $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION)/config.h

	@echo 'struct udev* udev_new(void) { return 0; }' >> $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION)/src/evdev.c
	@echo 'struct udev* udev_unref(struct udev *u) { return 0; }' >> $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION)/src/evdev.c
	@echo 'struct udev_device* udev_device_new_from_syspath(struct udev *u, const char* p) { return 0; }' >> $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION)/src/evdev.c
	@echo 'const char* udev_device_get_devnode(struct udev_device *d) { return 0; }' >> $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION)/src/evdev.c
	@echo 'struct udev_device* udev_device_unref(struct udev_device *d) { return 0; }' >> $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION)/src/evdev.c

	sed -i 's/-ludev//g' $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION)/src/Makefile

	$(MAKE) -C $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION)
	$(MAKE) -C $(TMPDIR)/$(XF86_INPUT_EVDEV_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install