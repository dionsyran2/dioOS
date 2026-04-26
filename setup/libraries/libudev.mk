LIB_PACKAGES += build_libudev

LIBUDEV_ZERO_GIT = https://github.com/illiliti/libudev-zero.git

build_libudev: build_xorg
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBUDEV),y)
	$(MAKE) make_libudev
endif

else
	$(MAKE) make_libudev
endif
endif

make_libudev:
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
		pkgconfigdir=/usr/share/pkgconfig \
		install

	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/usr/lib/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/usr/lib/pkgconfig/ 2>/dev/null || true