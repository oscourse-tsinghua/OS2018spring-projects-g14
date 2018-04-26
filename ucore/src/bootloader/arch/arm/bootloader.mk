SRCFILES	+= $(filter %.c %.S, $(wildcard arch/${ARCH}/*))
T_CC_ALL_FLAGS	+= -Iarch/${ARCH} -I../glue-kern/arch/${ARCH} -DBOOTLOADER_BASE=$(ARCH_ARM_BOOTLOADER_BASE) $(PLATFORM_DEF)

BOOTLOADER := ${T_OBJ}/bootloader.bin
KERNEL_ELF := $(KTREE_OBJ_ROOT)/kernel-$(ARCH).elf
KERNEL_IMAGE := $(OBJPATH_ROOT)/kernel.img

# include ${T_BASE}/mk/compbl.mk
include ${T_BASE}/mk/template.mk

all: $(KERNEL_IMAGE)


LINK_FILE_IN	:= arch/${ARCH}/bootloader.ld.in
LINK_FILE     := $(T_OBJ)/bootloader.ld
SEDFLAGS	= s/TEXT_START/$(ARCH_ARM_BOOTLOADER_BASE)/


$(LINK_FILE): $(LINK_FILE_IN)
	@echo "creating linker script"
	@sed  "$(SEDFLAGS)" < $< > $@

$(BOOTLOADER).elf: ${OBJFILES} $(LINK_FILE)
	@echo LD $@
	${V}${LD} -T$(LINK_FILE) -o$@ ${OBJFILES}

$(BOOTLOADER): $(BOOTLOADER).elf
	$(OBJCOPY) -S -O binary $< $@

$(KERNEL_IMAGE): $(BOOTLOADER) $(KERNEL_ELF)
	rm -f $@
	dd if=$(BOOTLOADER) of=$@
	dd if=$(KERNEL_ELF) of=$@ seek=8 conv=notrunc
