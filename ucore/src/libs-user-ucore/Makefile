.PHONY: all clean

-include $(KCONFIG_AUTOCONFIG)

ULIB_OBJ_ROOT := $(OBJPATH_ROOT)/libs-user-ucore
ULIB_A := $(ULIB_OBJ_ROOT)/ulib.a

TARGET_CFLAGS := -I. -Icommon -Iarch/$(ARCH) -nostdinc -nostdlib -fno-builtin
obj-y := dir.o file.o malloc.o panic.o signal.o spipe.o \
				stdio.o string.o syscall.o thread.o ulib.o umain.o mod.o mount.o
obj-y += common/hash.o common/rand.o common/printfmt.o \
				common/string.o
obj-$(UCONFIG_GPU_ENABLE) += GLES/gl.o

include arch/$(ARCH)/include.mk

obj-y += $(addprefix arch/$(ARCH)/, $(ARCH_OBJS))
obj-initobj += $(addprefix arch/$(ARCH)/, $(ARCH_INITCODE_OBJ))
TARGET_CFLAGS += $(ARCH_CFLAGS)
TARGET_LDFLAGS += $(ARCH_LDFLAGS)

target-obj := $(addprefix $(ULIB_OBJ_ROOT)/, $(obj-y))
target-initobj := $(addprefix $(ULIB_OBJ_ROOT)/, $(obj-initobj))

all: $(target-initobj) $(ULIB_A)

$(ULIB_A): $(target-obj)
	$(TARGET_AR) -cr $@ $+

$(ULIB_OBJ_ROOT)/%.o: %.c | $(ULIB_OBJ_ROOT)
	$(TARGET_CC) $(TARGET_CFLAGS) -c -o $@ $<

$(ULIB_OBJ_ROOT)/%.o: %.S | $(ULIB_OBJ_ROOT)
	$(TARGET_CC) -D__ASSEMBLY__ $(TARGET_CFLAGS) -c -o $@ $<

$(ULIB_OBJ_ROOT):
	@mkdir -p $(ULIB_OBJ_ROOT)/common
	@mkdir -p $(ULIB_OBJ_ROOT)/arch/$(ARCH)
ifeq ($(UCONFIG_GPU_ENABLE),y)
	@mkdir -p $(ULIB_OBJ_ROOT)/GLES
endif

clean:
	find . -name \*.o -exec rm -f {} \;
	rm -f ulib.a
