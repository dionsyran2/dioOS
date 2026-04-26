LIB_PACKAGES += build_xorgproto

XORGPROTO_VERSION = xorgproto-2024.1
XORGPROTO_URL = https://www.x.org/archive/individual/proto/$(XORGPROTO_VERSION).tar.xz

build_xorgproto: build_glibc build_util_macros 
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_XORGPROTO),y)
	$(MAKE) make_xorgproto
endif

else
	$(MAKE) make_xorgproto
endif
endif

make_xorgproto:
	@echo "Building xorgproto..."
	@if [ ! -f $(TMPDIR)/$(XORGPROTO_VERSION).tar.xz ]; then \
		wget -P $(TMPDIR) $(XORGPROTO_URL); \
	fi

	@if [ ! -d $(TMPDIR)/$(XORGPROTO_VERSION) ]; then \
		tar -C $(TMPDIR) -xf $(TMPDIR)/$(XORGPROTO_VERSION).tar.xz; \
	fi

	cd $(TMPDIR)/$(XORGPROTO_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	./configure \
		--prefix=/usr \
		--host=$(TARGET_ARCH) \
		--with-pkgconfigdir=/usr/share/pkgconfig


	$(MAKE) -C $(TMPDIR)/$(XORGPROTO_VERSION)
	$(MAKE) -C $(TMPDIR)/$(XORGPROTO_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install