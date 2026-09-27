# devkitPro 3DS homebrew Makefile
ifeq ($(strip $(DEVKITPRO)),)
$(error "DEVKITPRO is not set. Install devkitPro and the 3DS development packages.")
endif

include $(DEVKITPRO)/devkitARM/base_rules

TARGET := LuckeeMiner
BUILD := build
SOURCES := source
INCLUDES := include

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections $(ARCH) $(INCLUDE) -D__3DS__
CXXFLAGS := $(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lcitro3d -lctru -lm

.PHONY: all clean

all: $(TARGET).3dsx

$(TARGET).elf: $(foreach dir,$(SOURCES),$(patsubst $(dir)/%.cpp,$(BUILD)/%.o,$(wildcard $(dir)/*.cpp)))
	@echo linking $(notdir $@)
	$(CXX) $(LDFLAGS) $^ $(LIBS) -o $@

$(TARGET).3dsx: $(TARGET).elf
	$(3DSX) $< $@

$(BUILD)/%.o: $(SOURCES)/%.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(INCLUDES) -c $< -o $@

clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh