# Paths and flags come from the verified toolchain lock, via build-ps3.
ifndef PPU_CC
$(error PPU_CC is required; use scripts/build-ps3 after a successful setup)
endif

.PHONY: all
all: ../../build/ps3/hello.elf

../../build/ps3/hello.elf: main.c Makefile
	mkdir -p ../../build/ps3
	$(PPU_CC) $(PPU_CFLAGS) main.c $(PPU_LDFLAGS) -o $@
