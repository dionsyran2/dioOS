PACKAGES += build_sudo

SUDO_VERSION = sudo-1.9.17p2
SUDO_TARBALL = $(SUDO_VERSION).tar.gz
SUDO_URL = https://www.sudo.ws/dist/$(SUDO_TARBALL)


build_sudo:
ifeq ($(CONFIG_SUDO),y)

ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_SUDO),y)
	$(MAKE) make_sudo
endif

else
	$(MAKE) make_sudo
endif
endif

make_sudo:
	@echo "Building sudo..."
	@if [ ! -f $(TMPDIR)/$(SUDO_TARBALL) ]; then wget -P $(TMPDIR) $(SUDO_URL); fi
	@if [ ! -d $(TMPDIR)/$(SUDO_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(SUDO_TARBALL); fi
    
	cd $(TMPDIR)/$(SUDO_VERSION) && \
	./configure --prefix=/usr --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR)
	$(MAKE) -C $(TMPDIR)/$(SUDO_VERSION) -j$(NPROC)
	sudo $(MAKE) -C $(TMPDIR)/$(SUDO_VERSION) DESTDIR=$(DISKDIR) install