PACKAGES += build_bash

BASH_VERSION = 	bash-5.3
BASH_TARBALL = $(BASH_VERSION).tar.gz
BASH_URL = $(GNU_MIRROR)/gnu/bash/$(BASH_TARBALL)

build_bash:
ifeq ($(CONFIG_BASH),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_BASH),y)
	$(MAKE) make_bash
endif

else
	$(MAKE) make_bash
endif
endif

make_bash:
	@echo "Building bash..."
	@if [ ! -f $(TMPDIR)/$(BASH_TARBALL) ]; then \
		wget -P $(TMPDIR) $(BASH_URL); \
	fi

	@if [ ! -d $(TMPDIR)/$(BASH_VERSION) ]; then \
		tar -C $(TMPDIR) -xf $(TMPDIR)/$(BASH_TARBALL); \
	fi

	@if [ -f $(DISKDIR)/usr/bin/bash ]; then \
		rm $(DISKDIR)/usr/bin/bash; \
	fi

	cd $(TMPDIR)/$(BASH_VERSION) && \
	./configure \
		--prefix=/usr \
		--build=x86_64-host-linux-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		PKGCONFIGDIR=/usr/share/pkgconfig

	$(MAKE) -C $(TMPDIR)/$(BASH_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(BASH_VERSION) DESTDIR=$(DISKDIR) install