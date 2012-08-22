LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

GENPLUS_SRC_DIR := ../src
LIBRETRO_DIR	:= ../libretro

LOCAL_MODULE    := libretro
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
			$(GENPLUS_SRC_DIR)/input_hw/xe_a1p.c \
			$(GENPLUS_SRC_DIR)/input_hw/terebi_oekaki.c \
			$(GENPLUS_SRC_DIR)/cart_hw/areplay.c \
			$(GENPLUS_SRC_DIR)/cart_hw/md_cart.c \
			$(GENPLUS_SRC_DIR)/cart_hw/sms_cart.c \
			$(GENPLUS_SRC_DIR)/cart_hw/gg_eeprom.c \
			$(GENPLUS_SRC_DIR)/cart_hw/md_eeprom.c \
			$(GENPLUS_SRC_DIR)/cart_hw/ggenie.c \
			$(GENPLUS_SRC_DIR)/cart_hw/sram.c \
			$(GENPLUS_SRC_DIR)/cart_hw/svp/ssp16.c \
			$(GENPLUS_SRC_DIR)/cart_hw/svp/svp.c \
			$(GENPLUS_SRC_DIR)/ntsc/md_ntsc.c \
			$(GENPLUS_SRC_DIR)/ntsc/sms_ntsc.c \
			$(GENPLUS_SRC_DIR)/sound/Fir_Resampler.c \
			$(GENPLUS_SRC_DIR)/sound/eq.c \
			$(GENPLUS_SRC_DIR)/sound/sound.c \
			$(GENPLUS_SRC_DIR)/sound/ym2612.c \
			$(GENPLUS_SRC_DIR)/sound/ym2413.c \
			$(GENPLUS_SRC_DIR)/sound/sn76489.c \
			$(GENPLUS_SRC_DIR)/sound/blip.c \
			$(GENPLUS_SRC_DIR)/z80/z80.c \
			$(GENPLUS_SRC_DIR)/m68k/m68kcpu.c \
			$(LIBRETRO_DIR)/libretro.c

LOCAL_C_INCLUDES = $(LOCAL_PATH)/$(GENPLUS_SRC_DIR) \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/sound \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/input_hw \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/cart_hw \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/cart_hw/svp \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/m68k \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/z80 \
			$(LOCAL_PATH)/$(GENPLUS_SRC_DIR)/ntsc \
			$(LOCAL_PATH)/$(LIBRETRO_DIR)

LOCAL_CFLAGS = -DINLINE=inline -DUSE_15BPP_RENDERING -DLSB_FIRST -D__LIBRETRO__ 
include $(BUILD_SHARED_LIBRARY)
