LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

WANT_CRC32 := 1

CORE_DIR	:= ../../core
GENPLUS_SRC_DIR := $(CORE_DIR) \
					$(CORE_DIR)/sound \
					$(CORE_DIR)/input_hw \
					$(CORE_DIR)/cart_hw \
					$(CORE_DIR)/cart_hw/svp \
					$(CORE_DIR)/cd_hw \
					$(CORE_DIR)/m68k \
					$(CORE_DIR)/z80 \
					$(CORE_DIR)/ntsc
TREMOR_SRC_DIR  := $(CORE_DIR)/tremor
LIBRETRO_DIR	:= ..

LOCAL_MODULE    := retro

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
endif

SOURCES_C := $(foreach dir,$(GENPLUS_SRC_DIR),$(wildcard $(dir)/*.c)) \
				$(foreach dir,$(TREMOR_SRC_DIR),$(wildcard $(dir)/*.c)) \
				$(LIBRETRO_DIR)/libretro.c

ifeq ($(WANT_CRC32), 1)
	SOURCES_C += $(LIBRETRO_DIR)/scrc32.c
endif

LOCAL_SRC_FILES := $(SOURCES_C)

LOCAL_C_INCLUDES = $(CORE_DIR) \
			$(LOCAL_PATH)/$(CORE_DIR)/sound \
			$(LOCAL_PATH)/$(CORE_DIR)/input_hw \
			$(LOCAL_PATH)/$(CORE_DIR)/cd_hw \
			$(LOCAL_PATH)/$(CORE_DIR)/cart_hw \
			$(LOCAL_PATH)/$(CORE_DIR)/cart_hw/svp \
			$(LOCAL_PATH)/$(CORE_DIR)/m68k \
			$(LOCAL_PATH)/$(CORE_DIR)/z80 \
			$(LOCAL_PATH)/$(CORE_DIR)/ntsc \
			$(LOCAL_PATH)/$(TREMOR_SRC_DIR) \
			$(LOCAL_PATH)/$(LIBRETRO_DIR)

LOCAL_CFLAGS = -ffast-math -O2 -funroll-loops -DINLINE="static inline" -DUSE_LIBTREMOR -DUSE_16BPP_RENDERING -DLSB_FIRST -DBYTE_ORDER=LITTLE_ENDIAN -D__LIBRETRO__ -DFRONTEND_SUPPORTS_RGB565 -DALIGN_LONG -DALIGN_WORD

include $(BUILD_SHARED_LIBRARY)
