PACKAGES += build_vim

VIM_GIT = https://github.com/vim/vim.git
VIM_PATH := $(TMPDIR)/vim


build_vim:
ifeq ($(CONFIG_VIM),y)

ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_VIM),y)
	$(MAKE) make_vim
endif

else
	$(MAKE) make_vim
endif
endif

make_vim:
	@echo "Building vim..."
	@if [ ! -d $(VIM_PATH) ]; then \
		git clone $(VIM_GIT) $(VIM_PATH); \
	fi

	cd $(VIM_PATH) && \
	./configure \
		--prefix=/usr \
		--build=x86_64-host-linux-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR)

	$(MAKE) -C $(VIM_PATH) -j$(NPROC)
	$(MAKE) -C $(VIM_PATH) DESTDIR=$(DISKDIR) install