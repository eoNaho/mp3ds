# mp3ds - MP3 player para Nintendo 3DS (homebrew).  Requer devkitARM + libctru + citro2d/citro3d (grupo 3ds-dev).
.SUFFIXES:
ifneq ($(filter-out test clean,$(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)),)
ifeq ($(strip $(DEVKITARM)),)
$(error Defina DEVKITARM (ex.: export DEVKITPRO=/opt/devkitpro; export DEVKITARM=$$DEVKITPRO/devkitARM))
endif
include $(DEVKITARM)/3ds_rules
endif

TARGET      := mp3ds
APP_TITLE   := mp3ds
APP_DESC    := MP3 player leve para 3DS
APP_AUTHOR  := eoNaho
ICON        := icon.png

BUILD    := build
SRC_DIRS := source source/audio source/lib source/ui source/third_party
CFILES   := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.c))
OBJS     := $(patsubst %.c,$(BUILD)/%.o,$(CFILES))

LIBDIRS  := $(PORTLIBS) $(CTRULIB)
ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
INCLUDE  := $(addprefix -I,$(SRC_DIRS)) $(foreach d,$(LIBDIRS),-I$(d)/include)
LIBPATHS := $(foreach d,$(LIBDIRS),-L$(d)/lib)
CFLAGS   := -g -Wall -O2 -mword-relocations -ffunction-sections -MMD -MP $(ARCH) $(INCLUDE) -D__3DS__
LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(TARGET).map -Wl,--gc-sections
LIBS     := -lcitro2d -lcitro3d -lctru -lm

all: $(TARGET).3dsx

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBPATHS) $(LIBS) -o $@

$(TARGET).smdh: $(ICON)
	smdhtool --create "$(APP_TITLE)" "$(APP_DESC)" "$(APP_AUTHOR)" $(ICON) $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	3dsxtool $< $@ --smdh=$(TARGET).smdh

$(BUILD)/banner.bnr: cia/banner.png cia/silence.wav
	@mkdir -p $(BUILD)
	bannertool makebanner -i cia/banner.png -a cia/silence.wav -o $@

$(TARGET).cia: $(TARGET).elf $(TARGET).smdh $(BUILD)/banner.bnr cia/mp3ds.rsf
	makerom -f cia -o $@ -rsf cia/mp3ds.rsf -target t -exefslogo -elf $(TARGET).elf -icon $(TARGET).smdh -banner $(BUILD)/banner.bnr

cia: $(TARGET).cia

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).3dsx $(TARGET).cia $(TARGET).smdh $(TARGET).map

# Testes dos modulos portaveis (rodam no PC com gcc)
test:
	$(MAKE) -C tests

-include $(OBJS:.o=.d)

.PHONY: all cia clean test
