PACKAGES += build_xclock

XCLOCK_VERSION = xclock-1.1.1
XCLOCK_URL = https://www.x.org/releases/individual/app/$(XCLOCK_VERSION).tar.xz

build_xclock:
ifeq ($(CONFIG_X11),y)

ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_XCLOCK),y)
	$(MAKE) make_xclock
endif

else
	$(MAKE) make_xclock
endif
endif

make_xclock:
	@echo "Building xclock..."
	@if [ ! -f $(TMPDIR)/$(XCLOCK_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(XCLOCK_URL); fi
	@if [ ! -d $(TMPDIR)/$(XCLOCK_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XCLOCK_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(XCLOCK_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) LIBS="-lm"
	$(MAKE) -C $(TMPDIR)/$(XCLOCK_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(XCLOCK_VERSION) DESTDIR=$(DISKDIR) install


