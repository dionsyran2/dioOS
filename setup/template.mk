LIB_PACKAGES += build_template



build_template:
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_TEMPLATE),y)
	$(MAKE) make_template
endif

else
	$(MAKE) make_template
endif

make_template:
