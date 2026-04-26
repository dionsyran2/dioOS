LIB_PACKAGES += build_xtrans

XTRANS_VERSION = xtrans-1.5.0
XTRANS_URL = https://www.x.org/archive/individual/lib/$(XTRANS_VERSION).tar.xz


build_xtrans: build_glibc build_util_macros 
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_XTRANS),y)
	$(MAKE) make_xtrans
endif

else
	$(MAKE) make_xtrans
endif
endif

make_xtrans:
	@echo "Building xtrans..."
	@if [ ! -f $(TMPDIR)/$(XTRANS_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(XTRANS_URL); fi
	@if [ ! -d $(TMPDIR)/$(XTRANS_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XTRANS_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(XTRANS_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	./configure --prefix=/usr --host=$(TARGET_ARCH) --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/$(XTRANS_VERSION)
	$(MAKE) -C $(TMPDIR)/$(XTRANS_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install