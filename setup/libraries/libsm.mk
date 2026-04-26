# libSM (Session Management)
LIB_PACKAGES += build_libsm

LIBSM_VERSION = libSM-1.2.4
LIBSM_URL = https://www.x.org/releases/individual/lib/$(LIBSM_VERSION).tar.xz

build_libsm: build_xorg build_libudev
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBSM),y)
	$(MAKE) make_libsm
endif

else
	$(MAKE) make_libsm
endif
endif

make_libsm:
	@echo "Building libSM..."
	@if [ ! -f $(TMPDIR)/$(LIBSM_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBSM_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBSM_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBSM_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(LIBSM_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/$(LIBSM_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBSM_VERSION) DESTDIR=$(DISKDIR) pkgconfigdir=/usr/lib/pkgconfig install install

	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/usr/lib/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/usr/lib/pkgconfig/ 2>/dev/null || true