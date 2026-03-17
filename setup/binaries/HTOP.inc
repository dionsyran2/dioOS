HTOP_GIT = https://github.com/htop-dev/htop.git
HTOP_PATH := $(TMPDIR)/htop

build_htop:
ifeq ($(CONFIG_HTOP),y)
	@if [ ! -d $(HTOP_PATH) ]; then \
		git clone $(HTOP_GIT) $(HTOP_PATH); \
	fi

	cd $(HTOP_PATH); \
	./autogen.sh; \
	./configure CC="$(CC) $(TARGET_FLAGS)" \
	LDFLAGS="$(LDFLAGS_FLAGS)" --prefix=/usr \
	--libdir=/lib/x86_64-dioOS-gnu \
	--host=x86_64-pc-linux-gnu; \
	$(MAKE) -j$(NPROC); \
	$(MAKE) DESTDIR=$(DISKDIR) install
endif