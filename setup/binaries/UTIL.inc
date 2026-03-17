UTIL_LINUX_GIT = https://github.com/util-linux/util-linux.git
UTIL_LINUX_PATH := $(TMPDIR)/util-linux

build_utils:
	@if [ ! -d $(UTIL_LINUX_PATH) ]; then \
		git clone $(UTIL_LINUX_GIT) $(UTIL_LINUX_PATH); \
	fi

	cd $(UTIL_LINUX_PATH); \
	./autogen.sh; \
	mkdir build; \
	cd build; \
	../configure CC="$(CC) $(TARGET_FLAGS)" \
	LDFLAGS="$(LDFLAGS_FLAGS)" --disable-all-programs \
	--libdir=/lib/x86_64-dioOS-gnu \
	--host=x86_64-pc-linux-gnu \
	--enable-agetty --disable-nls; \
	$(MAKE) agetty; \
	$(MAKE) DESTDIR=$(DISKDIR) install