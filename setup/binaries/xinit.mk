PACKAGES += build_xinit

XINIT_VERSION = xinit-1.4.2
XINIT_URL = https://www.x.org/releases/individual/app/$(XINIT_VERSION).tar.gz


build_xinit:
ifeq ($(CONFIG_X11),y)

ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_XINIT),y)
	$(MAKE) make_xinit
endif

else
	$(MAKE) make_xinit
endif
endif

make_xinit:
	@echo "Building xinit..."
	@if [ ! -f $(TMPDIR)/$(XINIT_VERSION).tar.gz ]; then wget -P $(TMPDIR) $(XINIT_URL); fi
	@if [ ! -d $(TMPDIR)/$(XINIT_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XINIT_VERSION).tar.gz; fi
	
	cd $(TMPDIR)/$(XINIT_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--with-xinitdir=/etc/X11/xinit

	$(MAKE) -C $(TMPDIR)/$(XINIT_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(XINIT_VERSION) DESTDIR=$(DISKDIR) install