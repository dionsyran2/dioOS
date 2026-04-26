 GNU_MIRROR = https://mirror.clientvps.com

NPROC := $(shell nproc)

-include $(SETUPDIR)/.config

include $(SETUPDIR)/binaries/*.mk
include $(SETUPDIR)/libraries/*.mk

include $(SETUPDIR)/Tools.inc


TOOLCHAIN_DIR := $(SETUPDIR)/../toolchain
TARGET_ARCH := x86_64-linux-gnu

CROSS_GCC := $(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-gcc

# Get the paths using the cross-compiler itself
STDCXX_PATH := $(shell $(CROSS_GCC) -print-file-name=libstdc++.so.6)
LIBGCC_PATH := $(shell $(CROSS_GCC) -print-file-name=libgcc_s.so.1)

copy_cpp_libs:
	@echo "Locating toolchain C++ libraries..."
	@mkdir -p $(DISKDIR)/lib/x86_64-dioOS-gnu/
	
	@sudo cp -P $$(dirname $(STDCXX_PATH))/libstdc++.so* $(DISKDIR)/lib/x86_64-dioOS-gnu/
	@sudo cp -P $$(dirname $(LIBGCC_PATH))/libgcc_s.so* $(DISKDIR)/lib/x86_64-dioOS-gnu/
	
	@sudo chown -R $(shell id -u):$(shell id -g) $(DISKDIR)/lib/x86_64-dioOS-gnu/

.PHONY: setup
.SILENT: setup
setup:
	$(MAKE) -C $(KERNELDIR) setup

	@if [ ! -f $(CROSS_GCC) ]; then \
		$(MAKE) tools; \
	fi
	
	@printf "\e[32mRunning Setup\e[0m\n"

	@if [ ! -d $(TMPDIR) ]; then \
		mkdir -p $(TMPDIR); \
	fi
	
	@if [ ! -d $(DISKDIR) ]; then \
		mkdir -p $(DISKDIR); \
	fi

	@if [ ! -d $(DISKDIR)/bin ]; then \
		mkdir -p $(DISKDIR)/bin; \
	fi

	@if [ ! -d $(DISKDIR)/lib ]; then \
		mkdir -p $(DISKDIR)/lib; \
	fi

	@if [ ! -d $(DISKDIR)/etc ]; then \
		mkdir -p $(DISKDIR)/etc; \
	fi

	@if [ ! -d $(DISKDIR)/usr/include ]; then \
		mkdir -p $(DISKDIR)/usr/include; \
	fi

	@if [ ! -d $(DISKDIR)/usr/bin ]; then \
		mkdir -p $(DISKDIR)/usr/bin; \
	fi

	@if [ ! -d $(DISKDIR)/fonts ]; then \
		cp $(SETUPDIR)/fonts $(DISKDIR)/fonts -r; \
	fi

	cd $(SETUPDIR); \
	kconfig mconf dioOS_config

	cp $(SETUPDIR)/inittab $(DISKDIR)/etc/

	cp $(SETUPDIR)/profile $(DISKDIR)/etc/

	cp $(SETUPDIR)/include/* $(DISKDIR)/usr/include/ -r

	CC="$(CROSS_GCC)" \
	EXTRA_CFLAGS="--sysroot=$(DISKDIR)" \
	CXX="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-g++ --sysroot=$(DISKDIR) -nostdinc -I$(DISKDIR)/usr/include" \
	LD="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ld" \
	AR="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ar" \
	RANLIB="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ranlib" \
	NM="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-nm" \
	STRIP="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-strip" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/lib -L$(DISKDIR)/usr/lib -Wl,-rpath-link=$(DISKDIR)/lib/x86_64-dioOS-gnu"

	$(MAKE) $(LIB_PACKAGES)
	@echo "Library build complete: $(LIB_PACKAGES)"

	$(MAKE) $(PACKAGES)
	@echo "Binary build complete: $(PACKAGES)"

	@if [ ! -f $(DISKDIR)/etc/passwd ]; then \
	 	python3 $(SETUPDIR)/user_creator.py; \
	fi

	rm -rf $(TMPDIR)

	@if [ ! -f $(DISKDIR)/bin/bash ]; then \
		ln -s /usr/bin/bash $(DISKDIR)/bin/bash; \
	fi

	$(MAKE) copy_cpp_libs

	@sudo chown -R $(id -u):$(id -g) $(DISKDIR)

	@sudo find $(DISKDIR) -type d -exec chmod 755 {} +

	@sudo find $(DISKDIR) -type f -name "*" -executable -exec chmod 755 {} +

	@if [ -f $(DISKDIR)/bin/busybox ]; then \
		sudo chown root:root $(DISKDIR)/bin/busybox && \
		sudo chmod 4755 $(DISKDIR)/bin/busybox; \
	fi

	@if [ -f $(DISKDIR)/usr/bin/sudo ]; then \
		sudo chown root:root $(DISKDIR)/usr/bin/sudo && \
		sudo chmod 4755 $(DISKDIR)/usr/bin/sudo; \
	fi
	
	@cp $(SETUPDIR)/X11 $(DISKDIR)/etc/ -r

.SILENT: setup-help
setup-help:
	printf "\e[1mdioOS Setup Flags: \e[0m\n"
	printf "DISABLE_ALL=y       \t It will disable every build recipe. You should pair it with a specific recipe flag! \n"

	printf "\n\e[1mSpecific recipe flags (Pair them with DISABLE_ALL=y):\e[0m\n"
	printf "BUILD_LIMINE=y      \t Enable the limine recipe\n"

# Libs
	printf "\n"
	printf "BUILD_GLIBC=y        \t Enable the glibc recipe\n"
	printf "BUILD_NCURSES=y      \t Enable the ncurses recipe\n"
	printf "BUILD_LIBCAP=y       \t Enable the libcap recipe\n"
	printf "BUILD_ZLIB=y         \t Enable the zlib recipe\n"
	printf "BUILD_LIBFFI=y       \t Enable the libffi recipe\n"
	printf "BUILD_OPENSSL=y      \t Enable the openssl recipe\n"
	printf "BUILD_GLIB=y         \t Enable the glib recipe\n"
	printf "BUILD_LIBXML2=y      \t Enable the libxml2 recipe\n"
	printf "BUILD_UTIL_MACROS=y  \t Enable the util-macros recipe\n"
	printf "BUILD_XORGPROTO=y    \t Enable the xorgproto recipe\n"
	printf "BUILD_XTRANS=y       \t Enable the xtrans recipe\n"
	printf "BUILD_LIBXAU=y       \t Enable the libxau recipe\n"
	printf "BUILD_LIBXCB=y       \t Enable the libxcb recipe\n"
	printf "BUILD_XCBPROTO=y     \t Enable the xcbproto recipe\n"
	printf "BUILD_LIBX11=y       \t Enable the libX11 recipe\n"
	printf "BUILD_LIBXEXT=y      \t Enable the libXext recipe\n"
	printf "BUILD_PIXMAN=y       \t Enable the pixman recipe\n"
	printf "BUILD_LIBXKBFILE=y   \t Enable the libxkbfile recipe\n"
	printf "BUILD_LIBXCVT=y      \t Enable the libxcvt recipe\n"
	printf "BUILD_FREETYPE=y     \t Enable the freetype recipe\n"
	printf "BUILD_LIBFONTENC=y   \t Enable the libfontenc recipe\n"
	printf "BUILD_LIBXFONT=y     \t Enable the libxfont recipe\n"
	printf "BUILD_LIBXDMCP=y     \t Enable the libxdmcp recipe\n"
	printf "BUILD_LIBXRENDERER=y \t Enable the libxrenderer recipe\n"
	printf "BUILD_LIBPCIACCESS=y \t Enable the libpciaccess recipe\n"
	printf "BUILD_LIBPNG=y       \t Enable the libpng recipe\n"
	printf "BUILD_FBDEV=y        \t Enable the fbdev recipe\n"
	printf "BUILD_EVDEV=y        \t Enable the evdev recipe\n"
	printf "BUILD_LIBUDEV=y      \t Enable the libudev recipe\n"
	printf "BUILD_XKBCOMP=y      \t Enable the xkbcomp recipe\n"
	printf "BUILD_INPUT_KBD=y    \t Enable the input_kbd recipe\n"
	printf "BUILD_INPUT_MOUSE=y  \t Enable the input_mouse recipe\n"
	printf "BUILD_LIBICE=y       \t Enable the libICE recipe\n"
	printf "BUILD_LIBSM=y        \t Enable the libSM recipe\n"
	printf "BUILD_LIBXT=y        \t Enable the libXt recipe\n"
	printf "BUILD_LIBXMU=y       \t Enable the libXmu recipe\n"
	printf "BUILD_LIBXPM=y       \t Enable the libXpm recipe\n"
	printf "BUILD_LIBXFIXES=y    \t Enable the libXfixes recipe\n"
	printf "BUILD_LIBXCURSOR=y   \t Enable the libXcursor recipe\n"
	printf "BUILD_LIBXDAMAGE=y   \t Enable the libXdamage recipe\n"
	printf "BUILD_LIBXCOMPOSITE=y\t Enable the libXcomposite recipe\n"
	printf "BUILD_FONTCONFIG=y   \t Enable the fontconfig recipe\n"
	printf "BUILD_LIBXRANDR=y    \t Enable the libXrandr recipe\n"
	printf "BUILD_LIBXFT=y       \t Enable the libXft recipe\n"
	printf "BUILD_LIBXINERAMA=y  \t Enable the libXinerama recipe\n"
	printf "BUILD_IMLIB2=y       \t Enable the imlib2 recipe\n"
	printf "BUILD_LIBXAW=y       \t Enable the linXaw (Athena Widgets) recipe\n"
	printf "BUILD_GDK2=y         \t Enable the gdk2 recipe\n"
	printf "BUILD_LIBTIFF=y      \t Enable the libtiff recipe\n"
	printf "BUILD_LIBJPEG=y      \t Enable the libjpeg recipe\n"
	printf "BUILD_CAIRO=y        \t Enable the cairo recipe\n"
	printf "BUILD_PANGO=y        \t Enable the pango recipe\n"
	printf "BUILD_ACTUAL_FONTS=y \t Enable the fonts recipe\n"
	printf "BUILD_MKFONTSCALE=y  \t Enable the mkfontscale recipe\n"




# Bins
	printf "\n"
	printf "BUILD_BUSYBOX=y      \t Enable the busybox recipe\n"
	printf "BUILD_PYTHON3=y      \t Enable the python3 recipe\n"
	printf "BUILD_BASH=y         \t Enable the bash recipe\n"
	printf "BUILD_SUDO=y         \t Enable the sudo recipe\n"
	printf "BUILD_VIM=y          \t Enable the vim recipe\n"
	printf "BUILD_UTIL=y         \t Enable the util_linux (agetty) recipe\n"
	printf "BUILD_EXPAT=y        \t Enable the expat recipe\n"
	printf "BUILD_XORG=y         \t Enable the Xorg recipe\n"	
	printf "BUILD_XINIT=y        \t Enable the Xinit recipe\n"
	printf "BUILD_XCLOCK=y       \t Enable the Xclock recipe\n"
	printf "BUILD_XTERM=y        \t Enable the Xterm recipe\n"
	printf "BUILD_TWM=y          \t Enable the TWM recipe\n"
	printf "BUILD_ICEWM=y        \t Enable the icewm recipe\n"
	printf "BUILD_LEAFPAD=y      \t Enable the leafpad recipe\n"





cleansetup:
	rm -rf $(TMPDIR)
