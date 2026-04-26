LIB_PACKAGES += build_libxau

LIBXAU_VERSION = libXau-1.0.11
LIBXAU_URL = https://www.x.org/archive/individual/lib/$(LIBXAU_VERSION).tar.xz

build_libxau: build_glibc build_util_macros 
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXAU),y)
	$(MAKE) make_libxau
endif

else
	$(MAKE) make_libxau
endif
endif

make_libxau:
	@echo "Building libXau..."
	@if [ ! -f $(TMPDIR)/$(LIBXAU_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXAU_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXAU_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXAU_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(LIBXAU_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-pkgconfigdir=/usr/share/pkgconfig

	$(MAKE) -C $(TMPDIR)/$(LIBXAU_VERSION)
	$(MAKE) -C $(TMPDIR)/$(LIBXAU_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install