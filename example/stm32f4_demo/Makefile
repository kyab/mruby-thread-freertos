# Try "make help" for more information on BOARD and MEMORY_TARGET;
# these default to a Maple Flash build.
#BOARD ?= maple
#BOARD ?= aeroquad32
#BOARD ?= aeroquad32f1
BOARD ?= discovery_f4
#BOARD ?= aeroquad32mini
#BOARD ?= freeflight

#V=1

.DEFAULT_GOAL := sketch

LIB_MAPLE_HOME ?= /Users/koji/tools/AeroQuad/Libmaple/libmaple
MRUBY_HOME ?= /Users/koji/work/mruby/mruby-sharedvm

MRUBY_INCLUDES = -I$(MRUBY_HOME)/include
MRUBY_LIB = -L$(MRUBY_HOME)/build/STM32F4/lib

##
## Useful paths, constants, etc.
##

ifeq ($(LIB_MAPLE_HOME),)
SRCROOT := .
else
SRCROOT := $(LIB_MAPLE_HOME)
endif
BUILD_PATH = build
LIBMAPLE_PATH := $(SRCROOT)/libmaple
WIRISH_PATH := $(SRCROOT)/wirish
SUPPORT_PATH := $(SRCROOT)/support
# Support files for linker
LDDIR := $(SUPPORT_PATH)/ld
# Support files for this Makefile
MAKEDIR := $(SUPPORT_PATH)/make

# USB ID for DFU upload
VENDOR_ID  := 1EAF
PRODUCT_ID := 0003

##
## Target-specific configuration.  This determines some compiler and
## linker options/flags.
##

MEMORY_TARGET ?= jtag

# $(BOARD)- and $(MEMORY_TARGET)-specific configuration
include $(MAKEDIR)/target-config.mk

##
## Compilation flags
##

GLOBAL_FLAGS    := -D$(VECT_BASE_ADDR)					     \
		   -DBOARD_$(BOARD) -DMCU_$(MCU)			     \
		   -DERROR_LED_PORT=$(ERROR_LED_PORT)			     \
		   -DERROR_LED_PIN=$(ERROR_LED_PIN)			     \
		   -D$(DENSITY) -D$(MCU_FAMILY) 

ifeq ($(BOARD), freeflight)
GLOBAL_FLAGS += -DDISABLEUSB
endif

ifeq ($(BOARD), aeroquad32)
GLOBAL_FLAGS += -DF_CPU=168000000UL
endif

ifeq ($(BOARD), discovery_f4)
GLOBAL_FLAGS += -DF_CPU=168000000UL
endif

GLOBAL_FLAGS += -DENABLE_THREAD

GLOBAL_FLAGS += -D__FPU_PRESENT=1

ifeq ($(MCU_FAMILY), STM32F2)
	EXTRAINCDIRS += \
		$(LIB_MAPLE_HOME)/libmaple/usbF4/STM32_USB_Device_Library/Core/inc \
		$(LIB_MAPLE_HOME)/libmaple/usbF4/STM32_USB_Device_Library/Class/cdc/inc \
		$(LIB_MAPLE_HOME)/libmaple/usbF4/STM32_USB_OTG_Driver/inc \
		$(LIB_MAPLE_HOME)/libmaple/usbF4/VCP
endif

		   
#GLOBAL_FLAGS += -DDISABLEUSB
#GLOBAL_FLAGS += -DUSB_DISC_OD
		   
GLOBAL_CFLAGS   := -Os -g3 -gdwarf-2  -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=softfp \
		   -nostdlib -ffunction-sections -fdata-sections	     \
		   -Wl,--gc-sections $(GLOBAL_FLAGS)
GLOBAL_CXXFLAGS := -fno-rtti -fno-exceptions -Wall $(GLOBAL_FLAGS)
GLOBAL_ASFLAGS  := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=softfp		     \
		   -x assembler-with-cpp $(GLOBAL_FLAGS)
LDFLAGS  = -T$(LDDIR)/$(LDSCRIPT) -L$(LDDIR)    \
            -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=softfp -Xlinker     \
            --gc-sections -Wall -Xlinker --allow-multiple-definition

## set stack to additional CCM data RAM(64K)
LDFLAGS += -Xlinker -defsym -Xlinker __cs3_stack=0x10000000+64K


##
## Build rules and useful templates
##

include $(SUPPORT_PATH)/make/build-rules.mk
include $(SUPPORT_PATH)/make/build-templates.mk

##
## Set all submodules here
##

# Try to keep LIBMAPLE_MODULES a simply-expanded variable
ifeq ($(LIBMAPLE_MODULES),)
	LIBMAPLE_MODULES := $(SRCROOT)/libmaple
else
	LIBMAPLE_MODULES += $(SRCROOT)/libmaple
endif
LIBMAPLE_MODULES += $(SRCROOT)/wirish
# Official libraries:
LIBMAPLE_MODULES += $(SRCROOT)/libraries/Servo
LIBMAPLE_MODULES += $(SRCROOT)/libraries/LiquidCrystal
LIBMAPLE_MODULES += $(SRCROOT)/libraries/Wire

# Experimental libraries:
LIBMAPLE_MODULES += $(SRCROOT)/libraries/FreeRTOS
#LIBMAPLE_MODULES += $(SRCROOT)/libraries/mapleSDfat

# Call each module's rules.mk:
$(foreach m,$(LIBMAPLE_MODULES),$(eval $(call LIBMAPLE_MODULE_template,$(m))))

##
## Targets
##

# main target
include $(SRCROOT)/build-targets.mk

$(BUILD_PATH)/$(BOARD).elf: $(BUILDDIRS) $(TGT_BIN) $(BUILD_PATH)/main.o $(BUILD_PATH)/blinker.o mruby
	$(SILENT_LD) $(CXX) $(LDFLAGS) -o $@ $(BUILD_PATH)/main.o $(BUILD_PATH)/blinker.o $(TGT_BIN) $(MRUBY_LIB) -lmruby -Wl,-Map,$(BUILD_PATH)/$(BOARD).map

WIRISH_INCLUDES += -I$(SRCROOT)/libraries

build_dir : $(BUILD_PATH)
	mkdir -p $<

$(BUILD_PATH)/main.o: main.cpp
	$(CXX) $(CFLAGS) $(CXXFLAGS) $(LIBMAPLE_INCLUDES) $(WIRISH_INCLUDES) $(MRUBY_INCLUDES) -o $@ -c $< 

blinker.c: blinker.rb mruby
	$(MRUBY_HOME)/build/host/bin/mrbc -Bblinker $<

$(BUILD_PATH)/blinker.o: blinker.c
	$(CC) $(CFLAGS) -o $@ -c $< 

.PHONY: install sketch clean help debug cscope tags ctags ram flash jtag doxygen mrproper mruby cleanall upload

mruby:
	cd $(MRUBY_HOME); ./minirake

# Target upload commands
UPLOAD_ram   := $(SUPPORT_PATH)/scripts/reset.py && \
                sleep 1                  && \
                $(DFU) -a0 -d $(VENDOR_ID):$(PRODUCT_ID) -D $(BUILD_PATH)/$(BOARD).bin -R
UPLOAD_flash := $(SUPPORT_PATH)/scripts/reset.py && \
                sleep 1                  && \
                $(DFU) -a1 -d $(VENDOR_ID):$(PRODUCT_ID) -D $(BUILD_PATH)/$(BOARD).bin -R
UPLOAD_jtag  := $(OPENOCD_WRAPPER) flash

all: library

# Conditionally upload to whatever the last build was
install: INSTALL_TARGET = $(shell cat $(BUILD_PATH)/build-type 2>/dev/null)
install: $(BUILD_PATH)/$(BOARD).bin
	@echo Install target: $(INSTALL_TARGET)
	$(UPLOAD_$(INSTALL_TARGET))

# Force a rebuild if the target changed
PREV_BUILD_TYPE = $(shell cat $(BUILD_PATH)/build-type 2>/dev/null)
build-check:
ifneq ($(PREV_BUILD_TYPE), $(MEMORY_TARGET))
	$(shell rm -rf $(BUILD_PATH))
endif

sketch: build-check MSG_INFO $(BUILD_PATH)/$(BOARD).bin

clean:
	rm -rf build
	rm -f blinker.c

cleanall: clean
	cd $(MRUBY_HOME); ./minirake clean

upload: sketch
	st-flash write $(BUILD_PATH)/$(BOARD).bin 0x08000000

mrproper: clean
	rm -rf doxygen

help:
	@echo ""
	@echo "  libmaple Makefile help"
	@echo "  ----------------------"
	@echo "  "
	@echo "  Programming targets:"
	@echo "      sketch:   Compile for BOARD to MEMORY_TARGET (default)."
	@echo "      install:  Compile and upload code over USB, using Maple bootloader"
	@echo "  "
	@echo "  You *must* set BOARD if not compiling for Maple (e.g."
	@echo "  use BOARD=maple_mini for mini, etc.), and MEMORY_TARGET"
	@echo "  if not compiling to Flash."
	@echo "  "
	@echo "  Valid BOARDs:"
	@echo "      maple, maple_mini, maple_RET6, maple_native"
	@echo "  "
	@echo "  Valid MEMORY_TARGETs (default=flash):"
	@echo "      ram:    Compile sketch code to ram"
	@echo "      flash:  Compile sketch code to flash"
	@echo "      jtag:   Compile sketch code for jtag; overwrites bootloader"
	@echo "  "
	@echo "  Other targets:"
	@echo "      debug:  Start OpenOCD gdb server on port 3333, telnet on port 4444"
	@echo "      clean: Remove all build and object files"
	@echo "      help: Show this message"
	@echo "      doxygen: Build Doxygen HTML and XML documentation"
	@echo "      mrproper: Remove all generated files"
	@echo "  "

debug:
	$(OPENOCD_WRAPPER) debug

cscope:
	rm -rf *.cscope
	find . -name '*.[hcS]' -o -name '*.cpp' | xargs cscope -b

tags:
	etags `find . -name "*.c" -o -name "*.cpp" -o -name "*.h"`
	@echo "Made TAGS file for EMACS code browsing"

ctags:
	ctags-exuberant -R .
	@echo "Made tags file for VIM code browsing"

ram:
	@$(MAKE) MEMORY_TARGET=ram --no-print-directory sketch

flash:
	@$(MAKE) MEMORY_TARGET=flash --no-print-directory sketch

jtag:
	@$(MAKE) MEMORY_TARGET=jtag --no-print-directory sketch

doxygen:
	doxygen $(SUPPORT_PATH)/doxygen/Doxyfile
