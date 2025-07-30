APPNAME=test
LDS = app.ld
CC = gcc
ASMC = nasm
LD = ld


CFLAGS = -g
ASMFLAGS = 
LDFLAGS = -Bsymbolic

SRCDIR := ./
OBJDIR := ../../lib/test
BUILDDIR = ../../bin

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SRC = $(call rwildcard,$(SRCDIR),*.cpp)

ASMSRC = $(call rwildcard,$(SRCDIR),*.asm)  


OBJS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRC))
OBJS += $(patsubst $(SRCDIR)/%.asm, $(OBJDIR)/%_asm.o, $(ASMSRC))



DIRS = $(wildcard $(SRCDIR)/*)

app: $(OBJS) link
	
$(OBJDIR)/%.o: $(INCLUDE_DIRS)/%.cpp
	@ echo !==== COMPILING $^
	@ mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@
	
$(OBJDIR)/%_asm.o: $(INCLUDE_DIRS)/%.asm
	@ echo !==== COMPILING $^
	@ mkdir -p $(@D)
	$(ASMC) $(ASMFLAGS) $^ -f elf64 -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@ echo !==== COMPILING $^
	@ mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@
	
$(OBJDIR)/%_asm.o: $(SRCDIR)/%.asm
	@ echo !==== COMPILING $^
	@ mkdir -p $(@D)
	$(ASMC) $(ASMFLAGS) $^ -f elf64 -o $@
	 
link:
	@ echo !==== LINKING
	$(CC) -o $(BUILDDIR)/$(APPNAME).elf $(OBJS) -Wl,$(subst $(space),,$(LDFLAGS)) -Wl,--no-relax
