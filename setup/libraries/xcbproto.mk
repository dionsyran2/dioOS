LIB_PACKAGES += build_xcbproto

XCBPROTO_VERSION = xcb-proto-1.16.0
XCBPROTO_URL = https://xorg.freedesktop.org/archive/individual/proto/$(XCBPROTO_VERSION).tar.xz

build_xcbproto: build_glibc build_util_macros 
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_XCBPROTO),y)
	$(MAKE) make_xcbproto
endif

else
	$(MAKE) make_xcbproto
endif
endif

make_xcbproto:
	@echo "Building xcb-proto..."
	@if [ ! -f $(TMPDIR)/$(XCBPROTO_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(XCBPROTO_URL); fi
	@if [ ! -d $(TMPDIR)/$(XCBPROTO_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XCBPROTO_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(XCBPROTO_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	./configure \
		--prefix=/usr \
		--host=$(TARGET_ARCH) \
		--with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/$(XCBPROTO_VERSION)
	$(MAKE) -C $(TMPDIR)/$(XCBPROTO_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install