PACKAGES += build_binutils

BINUTILS_VERSION = binutils-2.40
BINUTILS_URL = https://ftp.gnu.org/gnu/binutils/$(BINUTILS_VERSION).tar.gz

build_binutils:
ifeq ($(DISABLE_ALL),y)
ifeq ($(BUILD_BINUTILS),y)
	$(MAKE) make_binutils
endif
else
	$(MAKE) make_binutils
endif

make_binutils:
	@echo "Building binutils for target..."
	@if [ ! -f $(TMPDIR)/$(BINUTILS_VERSION).tar.gz ]; then wget -P $(TMPDIR) $(BINUTILS_URL); fi
	@if [ ! -d $(TMPDIR)/$(BINUTILS_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(BINUTILS_VERSION).tar.gz; fi
	
	# Binutils strictly requires a separate build directory
	@mkdir -p $(TMPDIR)/build-binutils
	
	cd $(TMPDIR)/build-binutils && \
	../$(BINUTILS_VERSION)/configure \
		--prefix=/usr \
		--host=$(TARGET_ARCH) \
		--target=$(TARGET_ARCH) \
		--with-sysroot=/ \
		--disable-nls \
		--disable-werror
		
	$(MAKE) -C $(TMPDIR)/build-binutils -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/build-binutils DESTDIR=$(DISKDIR) install