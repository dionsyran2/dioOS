PACKAGES += build_gcc

TARG_GCC_VERSION = gcc-12.3.0
TARG_GCC_URL := https://ftp.gnu.org/gnu/gcc/$(TARG_GCC_VERSION)/$(TARG_GCC_VERSION).tar.xz

build_gcc: build_binutils 
ifeq ($(DISABLE_ALL),y)
ifeq ($(BUILD_GCC),y)
	$(MAKE) make_gcc
endif
else
	$(MAKE) make_gcc
endif

make_gcc:
	@echo "Building GCC for target..."
	@if [ ! -f $(TMPDIR)/$(TARG_GCC_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(TARG_GCC_URL); fi
	@if [ ! -d $(TMPDIR)/$(TARG_GCC_VERSION) ]; then \
		tar -C $(TMPDIR) -xf $(TMPDIR)/$(TARG_GCC_VERSION).tar.xz && \
		echo "Downloading GCC prerequisites (GMP, MPFR, MPC)..." && \
		cd $(TMPDIR)/$(TARG_GCC_VERSION) && ./contrib/download_prerequisites; \
	fi
	
	# GCC strictly requires a separate build directory
	@mkdir -p $(TMPDIR)/build-gcc
	
	cd $(TMPDIR)/build-gcc && \
	../$(TARG_GCC_VERSION)/configure \
		--prefix=/usr \
		--host=$(TARGET_ARCH) \
		--target=$(TARGET_ARCH) \
		--enable-languages=c,c++ \
		--disable-multilib \
		--disable-nls \
		--disable-bootstrap \
		--disable-werror

	$(MAKE) -C $(TMPDIR)/build-gcc -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/build-gcc DESTDIR=$(DISKDIR) install