 GNU_MIRROR = https://mirror.clientvps.com

NPROC := $(shell nproc)

-include $(SETUPDIR)/.config

include $(SETUPDIR)/binaries/BUSYBOX.inc

include $(SETUPDIR)/binaries/HTOP.inc

include $(SETUPDIR)/binaries/LIMINE.inc

include $(SETUPDIR)/binaries/UTIL.inc

include $(SETUPDIR)/binaries/BASH.inc

include $(SETUPDIR)/binaries/XORG.inc

include $(SETUPDIR)/libraries/GLIBC.inc

include $(SETUPDIR)/libraries/LIBCAP.inc

include $(SETUPDIR)/libraries/NCURSES.inc

include $(SETUPDIR)/libraries/X11.inc

include $(SETUPDIR)/Tools.inc


TOOLCHAIN_DIR := $(SETUPDIR)/../toolchain
TARGET_ARCH := x86_64-linux-gnu

CROSS_GCC := $(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-gcc

.PHONY: setup

setup:
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

	CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
	CXX="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-g++ --sysroot=$(DISKDIR)" \
	LD="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ld" \
	AR="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ar" \
	RANLIB="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ranlib" \
	NM="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-nm" \
	STRIP="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-strip" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/lib -L$(DISKDIR)/usr/lib -Wl,-rpath-link=$(DISKDIR)/lib/x86_64-dioOS-gnu"

	$(MAKE) build_limine

	$(MAKE) build_glibc
    
	$(MAKE) build_ncurses
	
	$(MAKE) build_libcap
	
	$(MAKE) build_busybox
	
	$(MAKE) build_htop
	
	$(MAKE) build_utils

	$(MAKE) build_bash
	
	$(MAKE) build_x11

	$(MAKE) build_xorg

	@if [ ! -f $(DISKDIR)/etc/passwd ]; then \
	 	python3 $(SETUPDIR)/user_creator.py; \
	fi

	rm -rf $(TMPDIR)


cleansetup:
	rm -rf $(TMPDIR)
