# LuckeeMiner - Nintendo 3DS homebrew
# Uses the standard devkitPro/libctru build rules so toolchain headers,
# dependency generation, linking, and .3dsx packaging are configured correctly.

ifeq ($(strip $(DEVKITPRO)),)
$(error "DEVKITPRO is not set. Install devkitPro and the 3DS development packages.")
endif

include $(DEVKITPRO)/libctru/3ds_rules

TARGET      := LuckeeMiner
BUILD       := build
SOURCES     := source
DATA        :=
INCLUDES    := include
GRAPHICS    :=

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS      := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections $(ARCH)
CXXFLAGS    := $(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions
ASFLAGS     := $(ARCH)
LDFLAGS     := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS        := -lctru -lm

.PHONY: all clean

all: $(TARGET).3dsx

clean:
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh
