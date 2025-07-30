# Default target
all: subdirs

# Find all subdirectories
SUBDIRS := $(wildcard src/*/)

# Target to call 'make' in each subdirectory
subdirs:
	@for dir in $(SUBDIRS); do \
		echo "Building in $$dir"; \
		$(MAKE) -C $$dir; \
	done