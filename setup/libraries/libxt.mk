# libXt (X Toolkit Intrinsics)

LIB_PACKAGES += build_libxt

LIBXT_VERSION = libXt-1.3.0
LIBXT_URL = https://www.x.org/releases/individual/lib/$(LIBXT_VERSION).tar.xz

build_libxt: build_xorg
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXT),y)
	$(MAKE) make_libxt
endif

else
	$(MAKE) make_libxt
endif
endif

make_libxt:
	@echo "Building libXt..."
	@if [ ! -f $(TMPDIR)/$(LIBXT_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXT_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXT_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXT_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(LIBXT_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --disable-malloc0returnsnull
	$(MAKE) -C $(TMPDIR)/$(LIBXT_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXT_VERSION) DESTDIR=$(DISKDIR) install

	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig/ 2>/dev/null || true