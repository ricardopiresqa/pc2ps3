# PC2PS3 PS3 build + package (runs inside the pc2ps3/ps3dev image).
#   docker run --rm -v "$PWD":/src -w /src/packaging/ps3 pc2ps3/ps3dev:1 make
# Outputs in dist/ps3/: pc2ps3.pkg (RPCS3), pc2ps3.gnpdrm.pkg (console with HEN),
# pc2ps3.elf and the unpacked layout (USRDIR/EBOOT.BIN, PARAM.SFO, ICON0, PIC1).
ifeq ($(strip $(PSL1GHT)),)
$(error PSL1GHT is not defined - run inside the pc2ps3/ps3dev Docker image)
endif

include $(PSL1GHT)/ppu_rules

ROOT       := $(abspath $(CURDIR)/../..)
TARGET     := pc2ps3
TITLE      := PC2PS3 Runtime Demo
# Own ID (no official title prefix such as BLUS/BLES/NPUB); must stay 9 chars.
APPID      := PCPS00001
CONTENTID  := UP0001-$(APPID)_00-0000000000000000
ICON0      := $(ROOT)/assets/phase0/ICON0.PNG
PKGFILES   := $(CURDIR)/pkgfiles
BUILDDIR   := $(ROOT)/build/ps3
OUTDIR     := $(ROOT)/dist/ps3

SDL2_CONFIG := $(PORTLIBS)/bin/sdl2-config
SOURCES := $(wildcard $(ROOT)/src/core/*.cpp) \
           $(ROOT)/src/platform/sdl_runtime_platform.cpp \
           $(ROOT)/src/platform/audio_callback.cpp \
           $(ROOT)/src/platform/framebuffer_presenter.cpp \
           $(ROOT)/src/platform/platform_input.cpp \
           $(ROOT)/src/platform/posix_file_system.cpp \
           $(ROOT)/apps/pc/runtime_demo_atlas.cpp \
           $(ROOT)/apps/ps3/main.cpp
# Optional local-only game module (games/cuphead is gitignored); absent in the public build.
LOCAL_GAME := $(wildcard $(ROOT)/games/cuphead/root_pack.cpp $(ROOT)/games/cuphead/root_pack_app.cpp)
# NO_LOCAL_GAME=1 builds the public slice even when the module exists.
ifneq ($(NO_LOCAL_GAME),)
LOCAL_GAME :=
endif
ifneq ($(LOCAL_GAME),)
$(warning Local game module compiled in: this PKG is for personal use only, do not distribute)
SOURCES += $(LOCAL_GAME)
LOCAL_GAME_FLAGS := -DPC2PS3_LOCAL_GAME -I$(ROOT)/games/cuphead
endif
OBJECTS := $(patsubst $(ROOT)/%.cpp,$(BUILDDIR)/obj/%.o,$(SOURCES))

CXXFLAGS := -O2 -Wall -std=gnu++11 -mcpu=cell $(MACHDEP) $(CXXFLAGS_INIT) \
            -I$(ROOT)/include -I$(ROOT)/apps/pc $(LOCAL_GAME_FLAGS) $(LIBPSL1GHT_INC) -I$(PORTLIBS)/include \
            $(shell $(SDL2_CONFIG) --cflags)
LDFLAGS  := $(MACHDEP) -L$(PORTLIBS)/lib $(LIBPSL1GHT_LIB)
LIBS     := $(shell $(SDL2_CONFIG) --libs) -lrsx -lgcm_sys -lio -lsysutil \
            -lrt -llv2 -laudio -lm

all: $(TARGET).pkg
	$(VERB) mkdir -p $(OUTDIR)
	$(VERB) cp $(TARGET).pkg $(TARGET).gnpdrm.pkg $(TARGET).elf $(OUTDIR)/
	$(VERB) rm -rf $(OUTDIR)/layout && cp -r $(BUILDDIR)/pkg $(OUTDIR)/layout
	$(VERB) cd $(OUTDIR) && sha256sum $(TARGET).pkg $(TARGET).gnpdrm.pkg $(TARGET).elf
	@echo "Packages ready: $(OUTDIR)"

$(PKGFILES)/PIC1.PNG: $(ROOT)/assets/phase0/PIC1.PNG
	$(VERB) mkdir -p $(PKGFILES)
	$(VERB) cp $< $@

$(TARGET).pkg: $(PKGFILES)/PIC1.PNG

$(BUILDDIR)/obj/%.o: $(ROOT)/%.cpp
	@echo $(notdir $<)
	$(VERB) mkdir -p $(dir $@)
	$(VERB) $(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJECTS)
	@echo linking ... $@
	$(VERB) $(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

clean:
	rm -rf $(BUILDDIR) $(PKGFILES) $(TARGET).elf $(TARGET).self \
	       $(TARGET).fake.self $(TARGET).pkg $(TARGET).gnpdrm.pkg

.PHONY: all clean
