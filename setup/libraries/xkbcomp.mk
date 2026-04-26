LIB_PACKAGES += build_xkbcomp

XKBCOMP_VERSION = xkbcomp-1.4.6
XKBCOMP_URL = https://www.x.org/releases/individual/app/$(XKBCOMP_VERSION).tar.xz

build_xkbcomp: build_xorg
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_XKBCOMP),y)
	$(MAKE) make_xkbcomp
endif

else
	$(MAKE) make_xkbcomp
endif
endif

make_xkbcomp:
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

	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/usr/lib/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/usr/lib/pkgconfig/ 2>/dev/null || true