PACKAGES += build_xterm

XTERM_VERSION = xterm-409
XTERM_URL = https://www.invisible-island.net/archives/xterm/$(XTERM_VERSION).tgz

build_xterm:
ifeq ($(CONFIG_X11),y)

ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_XTERM),y)
	$(MAKE) make_xterm
endif

else
	$(MAKE) make_xterm
endif
endif

make_xterm:
    ifeq ($(CONFIG_XTERM),y)
	@echo "Building xterm..."
	@if [ ! -f $(TMPDIR)/$(XTERM_VERSION).tgz ]; then wget -P $(TMPDIR) $(XTERM_URL); fi
	@if [ ! -d $(TMPDIR)/$(XTERM_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(XTERM_VERSION).tgz; fi
	
	cd $(TMPDIR)/$(XTERM_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
        --host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--x-includes=$(DISKDIR)/usr/include \
		--x-libraries=$(DISKDIR)/lib/x86_64-dioOS-gnu \
		--disable-setuid \
		--disable-setgid \
		--enable-mini-luit \
		--with-tty-group=tty \
		--disable-imake \
        CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
		CFLAGS="-I$(DISKDIR)/usr/include -O2" \
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link=$(DISKDIR)/lib/x86_64-dioOS-gnu" \
		LIBS="-lXaw -lXmu -lXt -lX11 -lXext -lxcb -lXau -lXdmcp -lICE -lSM -lncurses -ltinfo -lz"

	$(MAKE) -C $(TMPDIR)/$(XTERM_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(XTERM_VERSION) DESTDIR=$(DISKDIR) install
    endif
