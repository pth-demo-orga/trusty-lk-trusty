LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/mmutest_$(ARCH).S \

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

include make/module.mk
