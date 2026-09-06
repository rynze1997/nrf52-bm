TOOLCHAIN_PATH = arm-none-eabi-
CXX      = $(TOOLCHAIN_PATH)g++
OBJCOPY  = $(TOOLCHAIN_PATH)objcopy

MDK      = mdk
CMSIS    = CMSIS/Core/Include
BUILD    = build

# Last `make LED1=.. BAUD=..` so `make install` does not fall back to defaults.
-include $(BUILD)/.opts

# DK LED pads:
#   13 = P0.13 (LED1)   14 = P0.14 (LED2)
#   15 = P0.15 (LED3)   16 = P0.16 (LED4)
LED1 ?= 13
LED2 ?= 14
BAUD ?= 115200

# nRF UARTE register values, not arbitrary integers.
BAUDS = 1200 2400 4800 9600 14400 19200 28800 31250 38400 56000 57600 76800 115200 230400 250000 460800 921600 1000000

ifeq ($(filter $(LED1),13 14 15 16),)
$(error LED1 must be 13, 14, 15, or 16)
endif
ifeq ($(filter $(LED2),13 14 15 16),)
$(error LED2 must be 13, 14, 15, or 16)
endif
ifeq ($(LED1),$(LED2))
$(error LED1 and LED2 must use different pins)
endif
ifeq ($(filter $(BAUD),$(BAUDS)),)
$(error BAUD must be one of: $(BAUDS))
endif
ifeq ($(BAUD),1000000)
BAUD_SYM = Baud1M
else
BAUD_SYM = Baud$(BAUD)
endif

CXXFLAGS = -Os -std=c++17 -Wall -Wextra -Wpedantic \
           -mcpu=cortex-m4 -mfloat-abi=soft -ffreestanding -fno-exceptions -fno-rtti \
           -I$(MDK) -I$(CMSIS) -DLED1_PIN=$(LED1) -DLED2_PIN=$(LED2) \
           -DUARTE_BAUD=UARTE_BAUDRATE_BAUDRATE_$(BAUD_SYM)
LDFLAGS  = -T linker/nrf52840.ld -nostdlib -Wl,--print-memory-usage -Wl,-Map=$(BUILD)/firmware.map
LDLIBS   = -lc -lgcc
SRCS     = main.cpp startup/startup_nrf52840.S
HDRS     = parser.hpp fifo.hpp
CONFIG   = $(BUILD)/.config_$(LED1)_$(LED2)_$(BAUD)

all: $(BUILD)/firmware.hex

$(BUILD):
	mkdir -p $@

$(CONFIG): | $(BUILD)
	@rm -f $(BUILD)/.config_*
	@touch $@

$(BUILD)/firmware.elf: $(SRCS) $(HDRS) linker/nrf52840.ld $(CONFIG) | $(BUILD)
	@echo LED1=P0.$(LED1) LED2=P0.$(LED2) BAUD=$(BAUD)
	@printf 'LED1 := $(LED1)\nLED2 := $(LED2)\nBAUD := $(BAUD)\n' > $(BUILD)/.opts
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SRCS) $(LDLIBS)

$(BUILD)/firmware.hex: $(BUILD)/firmware.elf
	@$(OBJCOPY) -O ihex $< $@

install: $(BUILD)/firmware.hex
	nrfutil device recover
	nrfutil device program --firmware $< --options verify=VERIFY_READ,reset=RESET_SYSTEM

clean:
	rm -rf $(BUILD)

test: | $(BUILD)
	g++ -std=c++17 -Wall -Wextra -Werror -I. -o $(BUILD)/parser_test test/parser_test.cpp
	$(BUILD)/parser_test
	g++ -std=c++17 -Wall -Wextra -Werror -I. -o $(BUILD)/fifo_test test/fifo_test.cpp
	$(BUILD)/fifo_test

.PHONY: all install clean test
