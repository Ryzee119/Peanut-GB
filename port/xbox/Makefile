XBE_TITLE = Peanut-GB
GEN_XISO = $(XBE_TITLE).iso

NXDK_SDL = y

SRCS +=	main.c \
        ../../examples/sdl2/minigb_apu/minigb_apu.c

CFLAGS += -I../../examples/sdl2
CFLAGS += -O2
CFLAGS += -DENABLE_SOUND -DENABLE_SOUND_MINIGB
CFLAGS += -DENABLE_LCD
LDFLAGS += -stack:131072

new_all: copy_resources all

include $(NXDK_DIR)/Makefile

copy_resources:
	@mkdir -p $(OUTPUT_DIR)
	@mkdir -p $(OUTPUT_DIR)/Roms
	@mkdir -p $(OUTPUT_DIR)/Screenshots
	@mkdir -p $(OUTPUT_DIR)/Saves
	@cp font.ttf $(OUTPUT_DIR)/
