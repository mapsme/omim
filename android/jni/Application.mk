NDK_TOOLCHAIN_VERSION := clang3.6
APP_PLATFORM := android-15
APP_STL := c++_static

# libc++-specific issues: -std=c++11" is turned on by default.

# for gcc 4.8+
APP_CPPFLAGS += -Wno-deprecated-register

ifeq (x$(NDK_ABI_TO_BUILD), x)
  APP_ABI := armeabi-v7a-hard x86
else
  APP_ABI := $(NDK_ABI_TO_BUILD)
endif

LOCAL_PATH := $(call my-dir)
APP_CFLAGS += -I$(LOCAL_PATH)/../../3party/boost \
              -I$(LOCAL_PATH)/../../3party/protobuf/src

APP_GNUSTL_FORCE_CPP_FEATURES := exceptions rtti

ifeq ($(NDK_DEBUG),1)
  APP_OPTIM := debug
  APP_CFLAGS += -DDEBUG -D_DEBUG
else
  APP_OPTIM := release
  APP_CFLAGS += -DRELEASE -D_RELEASE
ifeq ($(PRODUCTION),1)
  APP_CFLAGS += -DOMIM_PRODUCTION
  # Temporary workaround for crashes on x86 arch when throwing C++ exceptions.
  # More details here: https://code.google.com/p/android/issues/detail?id=179410
  # TODO: Check if this workaround is needed in newer NDK versions (after current one r10e).
  LIBCXX_FORCE_REBUILD := trueifeq ($(PRODUCTION),1)
endif
endif
