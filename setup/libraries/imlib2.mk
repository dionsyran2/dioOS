LIB_PACKAGES += build_imlib2

IMLIB2_VERSION = 1.12.1
IMLIB2_URL = https://downloads.sourceforge.net/project/enlightenment/imlib2-src/$(IMLIB2_VERSION)/imlib2-$(IMLIB2_VERSION).tar.xz

build_imlib2: build_xorg build_libexpat
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_IMLIB2),y)
	$(MAKE) make_imlib2
endif

else
	$(MAKE) make_imlib2
endif
endif

make_imlib2:
	@echo "Building imlib2..."
	@if [ ! -f $(TMPDIR)/imlib2-$(IMLIB2_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(IMLIB2_URL); fi
	@if [ ! -d $(TMPDIR)/imlib2-$(IMLIB2_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/imlib2-$(IMLIB2_VERSION).tar.xz; fi
	cd $(TMPDIR)/imlib2-$(IMLIB2_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) \
	--disable-static --x-includes=$(DISKDIR)/usr/include --x-libraries=$(DISKDIR)/usr/lib --with-pkgconfigdir=/usr/share/pkgconfig

	$(MAKE) -C $(TMPDIR)/imlib2-$(IMLIB2_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/imlib2-$(IMLIB2_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install