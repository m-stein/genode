TARGET    = wireguard
REQUIRES += x86_64
LIBS     += base jitterentropy wireguard_linux
INC_DIR  += $(PRG_DIR)
SRC_CC   += main.cc
SRC_CC   += base64.cc
SRC_CC   += ipv4_address_prefix.cc
SRC_CC   += random.cc
SRC_C    += dummies.c
SRC_C    += glue.c
SRC_C    += lx_emul.c
SRC_C    += $(notdir $(wildcard $(PRG_DIR)/generated_dummies.c))

#
# lx_emul library
#
SRC_C   += lx_emul/clocksource.c
SRC_C   += lx_emul/irqchip.c
SRC_C   += lx_emul/shadow/fs/exec.c
SRC_C   += lx_emul/shadow/kernel/cpu.c
SRC_C   += lx_emul/shadow/kernel/fork.c
SRC_C   += lx_emul/shadow/kernel/pid.c
SRC_C   += lx_emul/shadow/kernel/printk/printk.c
SRC_C   += lx_emul/shadow/kernel/sched/core.c
SRC_C   += lx_emul/shadow/kernel/softirq.c
SRC_C   += lx_emul/shadow/mm/percpu.c
SRC_C   += lx_emul/shadow/mm/slab_common.c
SRC_C   += lx_emul/shadow/mm/slub.c
SRC_C   += lx_emul/spec/x86/start.c
SRC_C   += lx_emul/start.c
SRC_CC  += lx_emul/alloc.cc
SRC_CC  += lx_emul/clock.cc
SRC_CC  += lx_emul/debug.cc
SRC_CC  += lx_emul/init.cc
SRC_CC  += lx_emul/io_mem.cc
SRC_CC  += lx_emul/irq.cc
SRC_CC  += lx_emul/log.cc
SRC_CC  += lx_emul/page_virt.cc
SRC_CC  += lx_emul/task.cc
SRC_CC  += lx_emul/time.cc

SRC_CC  += lx_kit/console.cc
SRC_CC  += lx_kit/env.cc
SRC_CC  += lx_kit/init.cc
SRC_CC  += lx_kit/memory.cc
SRC_CC  += lx_kit/scheduler.cc
SRC_CC  += lx_kit/task.cc
SRC_CC  += lx_kit/timeout.cc
SRC_S   += lx_kit/spec/x86_64/setjmp.S

INC_DIR += $(REP_DIR)/src/include/lx_emul/shadow
INC_DIR += $(REP_DIR)/src/include/spec/x86_64
INC_DIR += $(REP_DIR)/src/include/spec/x86
INC_DIR += $(REP_DIR)/src/include

vpath lx_emul/% $(REP_DIR)/src/lib
vpath lx_kit/% $(REP_DIR)/src/lib

#
# Linux kernel code
#

LX_OUT_DIR := $(BUILD_BASE_DIR)/app/wireguard/linux

INC_DIR += $(LX_OUT_DIR)/arch/x86/include
INC_DIR += $(LX_OUT_DIR)/arch/x86/include/generated
INC_DIR += $(LX_OUT_DIR)/include
INC_DIR += $(LX_OUT_DIR)/arch/x86/include/uapi
INC_DIR += $(LX_OUT_DIR)/arch/x86/include/generated/uapi
INC_DIR += $(LX_OUT_DIR)/include/uapi
INC_DIR += $(LX_OUT_DIR)/include/generated/uapi

CC_C_OPT += -include $(LX_OUT_DIR)/include/linux/compiler-version.h
CC_C_OPT += -include $(LX_OUT_DIR)/include/linux/kconfig.h
CC_C_OPT += -include $(LX_OUT_DIR)/include/linux/compiler_types.h
CC_C_OPT += -D__KERNEL__ -Wall -Wundef -Werror=strict-prototypes
CC_C_OPT += -Wno-trigraphs
CC_C_OPT += -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type
CC_C_OPT += -Wno-format-security -std=gnu89 -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx
CC_C_OPT += -Wno-frame-address -Wno-format-truncation
CC_C_OPT += -Wno-format-overflow -O2
CC_C_OPT += -Wframe-larger-than=1024 -Wimplicit-fallthrough=5
CC_C_OPT += -Wno-main -Wno-unused-but-set-variable -Wno-unused-const-variable
CC_C_OPT += -Wdeclaration-after-statement -Wvla -Wno-pointer-sign -Wno-stringop-truncation
CC_C_OPT += -Wno-array-bounds -Wno-stringop-overflow -Wno-restrict -Wno-maybe-uninitialized
CC_C_OPT += -Werror=date-time
CC_C_OPT += -Werror=incompatible-pointer-types -Werror=designated-init
CC_C_OPT += -Wno-packed-not-aligned -D'pr_fmt(fmt)=KBUILD_MODNAME ": " fmt'

CC_OPT_drivers/net/wireguard/allowedips += -Wno-frame-larger-than

LX_SRC  = $(shell grep ".*\.c" $(PRG_DIR)/source.list)
SRC_S  += $(shell grep ".*\.S" $(PRG_DIR)/source.list)
SRC_C  += $(LX_SRC)

vpath %.c $(LX_OUT_DIR)
vpath %.S $(LX_OUT_DIR)

define CC_OPT_LX_RULES =
CC_OPT_$(1) += -DKBUILD_MODFILE='"$(1)"' -DKBUILD_BASENAME='"$(notdir $(1))"' -DKBUILD_MODNAME='"$(notdir $(1))"'
endef

MODULES := $(SRC_C:%.c=%)
$(foreach m,$(MODULES),$(eval $(call CC_OPT_LX_RULES,$(m))))
