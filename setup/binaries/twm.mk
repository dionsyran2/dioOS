PACKAGES += build_twm

TWM_VERSION = twm-1.0.12
TWM_URL = https://www.x.org/releases/individual/app/$(TWM_VERSION).tar.xz


build_twm:
ifeq ($(CONFIG_TWM),y)

ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_TWM),y)
	$(MAKE) make_twm
endif

else
	$(MAKE) make_twm
endif
endif

make_twm:
	@echo "Building twm..."
	@if [ ! -f $(TMPDIR)/$(TWM_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(TWM_URL); fi
	@if [ ! -d $(TMPDIR)/$(TWM_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(TWM_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(TWM_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR)
	$(MAKE) -C $(TMPDIR)/$(TWM_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(TWM_VERSION) DESTDIR=$(DISKDIR) install