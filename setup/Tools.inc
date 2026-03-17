TOOLCHAIN_TMP = $(TOOLCHAIN_DIR)/tmp

BINUTILS_VERSION := binutils-2.46.0
BINUTILS_TARBALL = $(BINUTILS_VERSION).tar.gz
BINUTILS_URL := $(GNU_MIRROR)/gnu/binutils/$(BINUTILS_TARBALL)
BINUTILS_TMP = $(TOOLCHAIN_TMP)/binutils
BINUTILS_BUILD_DIR = $(TOOLCHAIN_TMP)/$(BINUTILS_VERSION)/build

compile_binutils:
	@if [ ! -f $(TOOLCHAIN_TMP)/$(BINUTILS_TARBALL) ]; then \
		wget -P $(TOOLCHAIN_TMP) $(BINUTILS_URL); \
		tar -C $(TOOLCHAIN_TMP) -xf $(TOOLCHAIN_TMP)/$(BINUTILS_TARBALL); \
	fi

	mkdir -p $(BINUTILS_BUILD_DIR)
    
	cd $(BINUTILS_BUILD_DIR) && \
	../configure --target=$(TARGET_ARCH) --prefix="$(TOOLCHAIN_DIR)" --with-sysroot --disable-nls --disable-werror

	$(MAKE) -C $(BINUTILS_BUILD_DIR)
	$(MAKE) -C $(BINUTILS_BUILD_DIR) install


GCC_VERSION := 14.2.0
GCC_TARBALL := gcc-$(GCC_VERSION).tar.xz
GCC_URL = $(GNU_MIRROR)/gnu/gcc/gcc-$(GCC_VERSION)/$(GCC_TARBALL)
GCC_PATH = $(TOOLCHAIN_TMP)/gcc-$(GCC_VERSION)

compile_gcc:
	@if [ ! -f $(TOOLCHAIN_TMP)/$(GCC_TARBALL) ]; then \
		wget -P $(TOOLCHAIN_TMP) $(GCC_URL); \
	fi

	@if [ ! -d $(GCC_PATH) ]; then \
		tar -C $(TOOLCHAIN_TMP) -xf $(TOOLCHAIN_TMP)/$(GCC_TARBALL); \
	fi

	cd $(GCC_PATH) && ./contrib/download_prerequisites

	mkdir -p $(GCC_PATH)/build
	cd $(GCC_PATH)/build && \
	../configure \
	--target=$(TARGET_ARCH) \
	--prefix="$(TOOLCHAIN_DIR)" \
	--disable-nls \
	--enable-languages=c,c++ \
	--without-headers \
	--disable-hosted-libstdcxx \
	--disable-multilib

	$(MAKE) -C $(GCC_PATH)/build -j $(NPROC) all-gcc
	$(MAKE) -C $(GCC_PATH)/build -j $(NPROC) all-target-libgcc
	# $(MAKE) -C $(GCC_PATH)/build -j $(NPROC) all-target-libstdc++-v3
	$(MAKE) -C $(GCC_PATH)/build install-gcc
	$(MAKE) -C $(GCC_PATH)/build install-target-libgcc
	# $(MAKE) -C $(GCC_PATH)/build install-target-libstdc++-v3

tools:
	mkdir -p $(TOOLCHAIN_TMP)

	$(MAKE) compile_binutils
	$(MAKE) compile_gcc

	rm -rf $(TOOLCHAIN_TMP)

cleantools:
	rm -rf $(TOOLCHAIN_DIR)