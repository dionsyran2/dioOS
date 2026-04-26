LIB_PACKAGES += build_ncurses

NCURSES_VERSION = ncurses-6.3
NCURSES_TARBALL = $(NCURSES_VERSION).tar.gz
NCURSES_URL = $(GNU_MIRROR)/gnu/ncurses/$(NCURSES_TARBALL)
NCURSES_COMMON = 	--prefix=/usr \
					--libdir=/lib/x86_64-dioOS-gnu \
					--build=x86_64-host-linux-gnu \
					--host=$(TARGET_ARCH) \
					--with-sysroot=$(DISKDIR) \
					--with-termlib \
					--with-shared \
					--without-debug \
					--without-normal

build_ncurses: build_glibc
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_NCURSES),y)
	$(MAKE) make_ncurses
endif

else
	$(MAKE) make_ncurses
endif

make_ncurses:
		@echo "Building ncurses..."
	@if [ ! -f $(TMPDIR)/$(NCURSES_TARBALL) ]; then \
		wget -P $(TMPDIR) $(NCURSES_URL); \
	fi

	@if [ ! -d $(TMPDIR)/$(NCURSES_VERSION) ]; then \
		tar -C $(TMPDIR) -xf $(TMPDIR)/$(NCURSES_TARBALL); \
	fi

	# Non wide version
	cd $(TMPDIR)/$(NCURSES_VERSION) && \
	./configure $(NCURSES_COMMON) --disable-widec;

	$(MAKE) -C $(TMPDIR)/$(NCURSES_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(NCURSES_VERSION) DESTDIR=$(DISKDIR) install


	# Wide version
	cd $(TMPDIR)/$(NCURSES_VERSION) && \
	./configure $(NCURSES_COMMON) --enable-widec;

	$(MAKE) -C $(TMPDIR)/$(NCURSES_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(NCURSES_VERSION) DESTDIR=$(DISKDIR) install