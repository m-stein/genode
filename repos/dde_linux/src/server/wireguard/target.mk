TARGET    = wireguard_x86_64
REQUIRES  = x86_64
LIBS      = base

CONTRIB_DIR := /home/lypo/genodelabs/linux-5.15-rc2.wg_builds

INC_DIR += $(CONTRIB_DIR)/arch/x86/include
INC_DIR += $(CONTRIB_DIR)/arch/x86/include/generated
INC_DIR += $(CONTRIB_DIR)/include
INC_DIR += $(CONTRIB_DIR)/arch/x86/include/uapi
INC_DIR += $(CONTRIB_DIR)/arch/x86/include/generated/uapi
INC_DIR += $(CONTRIB_DIR)/include/uapi
INC_DIR += $(CONTRIB_DIR)/include/generated/uapi

CC_C_OPT += -Wp,-MMD,drivers/net/wireguard/.main.o.d
CC_C_OPT += -nostdinc
#CC_C_OPT += -isystem /usr/local/genode/tool/19.05/bin/../lib/gcc/x86_64-pc-elf/8.3.0/include
CC_C_OPT += -include $(CONTRIB_DIR)/include/linux/compiler-version.h -include
CC_C_OPT += $(CONTRIB_DIR)/include/linux/kconfig.h -include $(CONTRIB_DIR)/include/linux/compiler_types.h
CC_C_OPT += -D__KERNEL__ -fmacro-prefix-map=./= -Wall -Wundef -Werror=strict-prototypes
CC_C_OPT += -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -fno-PIE
CC_C_OPT += -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type
CC_C_OPT += -Wno-format-security -std=gnu89 -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx
CC_C_OPT += -fcf-protection=none -m64 -falign-jumps=1 -falign-loops=1 -mno-80387
CC_C_OPT += -mno-fp-ret-in-387 -mpreferred-stack-boundary=3 -mskip-rax-setup -mtune=generic
CC_C_OPT += -mno-red-zone -mcmodel=kernel -Wno-sign-compare -fno-asynchronous-unwind-tables
#CC_C_OPT += -mindirect-branch=thunk-extern
CC_C_OPT += -mindirect-branch-register -fno-jump-tables
CC_C_OPT += -fno-delete-null-pointer-checks -Wno-frame-address -Wno-format-truncation
CC_C_OPT += -Wno-format-overflow -O2
#CC_C_OPT += --param=allow-store-data-races=0
CC_C_OPT += -Wframe-larger-than=1024 -fno-stack-protector -Wimplicit-fallthrough=5
CC_C_OPT += -Wno-main -Wno-unused-but-set-variable -Wno-unused-const-variable
CC_C_OPT += -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-stack-clash-protection
CC_C_OPT += -g -gdwarf-4 -pg -mrecord-mcount -mfentry -DCC_USING_FENTRY
CC_C_OPT += -Wdeclaration-after-statement -Wvla -Wno-pointer-sign -Wno-stringop-truncation
CC_C_OPT += -Wno-array-bounds -Wno-stringop-overflow -Wno-restrict -Wno-maybe-uninitialized
CC_C_OPT += -fno-strict-overflow -fno-stack-check -fconserve-stack -Werror=date-time
CC_C_OPT += -Werror=incompatible-pointer-types -Werror=designated-init
CC_C_OPT += -Wno-packed-not-aligned -D'pr_fmt(fmt)=KBUILD_MODNAME ": " fmt'
CC_C_OPT += -DKBUILD_MODFILE='"drivers/net/wireguard/wireguard"'
CC_C_OPT += -DKBUILD_MODNAME='"wireguard"'
CC_C_OPT += -D__KBUILD_MODNAME=kmod_wireguard
#CC_C_OPT += -DDEBUG
CC_C_OPT += -c

LX_OBJECTS  = $(wildcard $(CONTRIB_DIR)/drivers/net/wireguard/*.o)
LX_REL_OBJ  = $(LX_OBJECTS:$(CONTRIB_DIR)/%=%)
SRC_C      += $(LX_REL_OBJ:%.o=%.c))

vpath %.c  $(CONTRIB_DIR)

define CC_OPT_LX_RULES =
CC_OPT_$(1) = -DKBUILD_BASENAME='"$(notdir $(1))"'
endef

$(foreach file,$(LX_REL_OBJ),$(eval $(call CC_OPT_LX_RULES,$(file:%.o=%))))
