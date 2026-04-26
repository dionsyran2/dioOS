PACKAGES += build_icewm

ICEWM_VERSION = 4.0.0
ICEWM_TARBALL = icewm-$(ICEWM_VERSION).tar.lz
ICEWM_URL = https://github.com/ice-wm/icewm/releases/download/$(ICEWM_VERSION)/$(ICEWM_TARBALL)


build_icewm:
ifeq ($(CONFIG_ICEWM),y)

ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_ICEWM),y)
	$(MAKE) make_icewm
endif

else
	$(MAKE) make_icewm
endif
endif

make_icewm:
	@echo "Building icewm..."
	@if [ ! -f $(TMPDIR)/$(ICEWM_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(ICEWM_URL); fi
	@if [ ! -d $(TMPDIR)/$(ICEWM_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(ICEWM_TARBALL); fi
	cd $(TMPDIR)/icewm-$(ICEWM_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --disable-fribidi
	$(MAKE) -C $(TMPDIR)/icewm-$(ICEWM_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/icewm-$(ICEWM_VERSION) DESTDIR=$(DISKDIR) install