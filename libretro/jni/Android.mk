LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

GENPLUS_SRC_DIR := ../../core
TREMOR_SRC_DIR := $(GENPLUS_SRC_DIR)/tremor
LIBRETRO_DIR	:= ../

LOCAL_MODULE    := retro

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
endif

LOCAL_SRC_FILES := $(GENPLUS_SRC_DIR)/genesis.c \
			$(GENPLUS_SRC_DIR)/vdp_ctrl.c \
			$(GENPLUS_SRC_DIR)/vdp_render.c \
			$(GENPLUS_SRC_DIR)/system.c \
			$(GENPLUS_SRC_DIR)/io_ctrl.c \
			$(GENPLUS_SRC_DIR)/loadrom.c \
			$(GENPLUS_SRC_DIR)/mem68k.c \
			$(GENPLUS_SRC_DIR)/state.c \
			$(GENPLUS_SRC_DIR)/memz80.c \
			$(GENPLUS_SRC_DIR)/membnk.c \
			$(GENPLUS_SRC_DIR)/input_hw/activator.c \
			$(GENPLUS_SRC_DIR)/input_hw/gamepad.c \
			$(GENPLUS_SRC_DIR)/input_hw/input.c \
			$(GENPLUS_SRC_DIR)/input_hw/lightgun.c \
			$(GENPLUS_SRC_DIR)/input_hw/mouse.c \
			$(GENPLUS_SRC_DIR)/input_hw/paddle.c \
			$(GENPLUS_SRC_DIR)/input_hw/sportspad.c \
			$(GENPLUS_SRC_DIR)/input_hw/teamplayer.c \
			$(GENPLUS_SRC_DIR)/input_hw/xe_1ap.c \
			$(GENPLUS_SRC_DIR)/input_hw/terebi_oekaki.c \
			$(GENPLUS_SRC_DIR)/input_hw/graphic_board.c \
			$(GENPLUS_SRC_DIR)/cd_hw/cd_cart.c \
			$(GENPLUS_SRC_DIR)/cd_hw/cdc.c \
			$(GENPLUS_SRC_DIR)/cd_hw/cdd.c \
			$(GENPLUS_SRC_DIR)/cd_hw/gfx.c \
			$(GENPLUS_SRC_DIR)/cd_hw/pcm.c \
			$(GENPLUS_SRC_DIR)/cd_hw/scd.c \
			$(GENPLUS_SRC_DIR)/cart_hw/areplay.c \
			$(GENPLUS_SRC_DIR)/cart_hw/md_cart.c \
			$(GENPLUS_SRC_DIR)/cart_hw/sms_cart.c \
			$(GENPLUS_SRC_DIR)/cart_hw/eeprom_93c.c \
			$(GENPLUS_SRC_DIR)/cart_hw/eeprom_i2c.c \
			$(GENPLUS_SRC_DIR)/cart_hw/eeprom_spi.c \
			$(GENPLUS_SRC_DIR)/cart_hw/ggenie.c \
			$(GENPLUS_SRC_DIR)/cart_hw/sram.c \
			$(GENPLUS_SRC_DIR)/cart_hw/svp/ssp16.c \
			$(GENPLUS_SRC_DIR)/cart_hw/svp/svp.c \
			$(GENPLUS_SRC_DIR)/ntsc/md_ntsc.c \
			$(GENPLUS_SRC_DIR)/ntsc/sms_ntsc.c \
			$(GENPLUS_SRC_DIR)/sound/eq.c \
			$(GENPLUS_SRC_DIR)/sound/sound.c \
			$(GENPLUS_SRC_DIR)/sound/ym2612.c \
			$(GENPLUS_SRC_DIR)/sound/ym2413.c \
			$(GENPLUS_SRC_DIR)/sound/sn76489.c \
			$(GENPLUS_SRC_DIR)/sound/blip_buf.c \
			$(GENPLUS_SRC_DIR)/z80/z80.c \
			$(GENPLUS_SRC_DIR)/m68k/m68kcpu.c \
			$(GENPLUS_SRC_DIR)/m68k/s68kcpu.c \
			$(TREMOR_SRC_DIR)/bitwise.c \
			$(TREMOR_SRC_DIR)/block.c \
			$(TREMOR_SRC_DIR)/codebook.c \
			$(TREMOR_SRC_DIR)/floor0.c \
			$(TREMOR_SRC_DIR)/floor1.c \
			$(TREMOR_SRC_DIR)/framing.c \
			$(TREMOR_SRC_DIR)/info.c \
			$(TREMOR_SRC_DIR)/mapping0.c \
			$(TREMOR_SRC_DIR)/mdct.c \
			$(TREMOR_SRC_DIR)/registry.c \
			$(TREMOR_SRC_DIR)/res012.c \
			$(TREMOR_SRC_DIR)/sharedbook.c \
			$(TREMOR_SRC_DIR)/synthesis.c \
			$(TREMOR_SRC_DIR)/vorbisfile.c \
			$(TREMOR_SRC_DIR)/window.c \
			$(LIBRETRO_DIR)/libretro.c \
			$(LIBRETRO_DIR)/scrc32.c

LOCAL_C_INCLUDES = $(LOCAL_PATH)/$(GENPLUS_SRC_DIR) \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/sound \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/input_hw \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/cd_hw \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/cart_hw \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/cart_hw/svp \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/m68k \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/z80 \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/ntsc \
			$(LOCAL_PATH)/$(TREMOR_SRC_DIR) \
			$(LOCAL_PATH)/$(LIBRETRO_DIR)

LOCAL_CFLAGS = -ffast-math -O2 -funroll-loops -DINLINE="static inline" -DUSE_LIBTREMOR -DUSE_16BPP_RENDERING -DLSB_FIRST -DBYTE_ORDER=LITTLE_ENDIAN -D__LIBRETRO__ -DFRONTEND_SUPPORTS_RGB565 -DALIGN_LONG -DALIGN_WORD

include $(BUILD_SHARED_LIBRARY)
