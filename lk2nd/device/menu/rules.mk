# SPDX-License-Identifier: BSD-3-Clause
LOCAL_DIR := $(GET_LOCAL_DIR)

ifeq ($(ENABLE_FBCON_DISPLAY_MSG),1)
$(error lk2nd device menu conflicts with ENABLE_FBCON_DISPLAY_MSG)
endif

ifeq ($(USE_ALTERNATIVE_MENU),1)
OBJS += $(LOCAL_DIR)/menu_alt.o
else
OBJS += $(LOCAL_DIR)/menu.o
endif
