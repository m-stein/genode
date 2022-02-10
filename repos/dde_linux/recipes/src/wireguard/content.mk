

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/wireguard)
PRG_DIR  := $(REP_DIR)/src/app/wireguard

MIRROR_FROM_REP_DIR := \
	lib/mk/spec/x86_64/wireguard.mk \
	src/app/wireguard \
	src/lib/lx_emul/clocksource.c \
	src/lib/lx_emul/irqchip.c \
	src/lib/lx_emul/shadow/fs/exec.c \
	src/lib/lx_emul/shadow/kernel/cpu.c \
	src/lib/lx_emul/shadow/kernel/exit.c \
	src/lib/lx_emul/shadow/kernel/fork.c \
	src/lib/lx_emul/shadow/kernel/pid.c \
	src/lib/lx_emul/shadow/kernel/printk/printk.c \
	src/lib/lx_emul/shadow/kernel/sched/core.c \
	src/lib/lx_emul/shadow/kernel/softirq.c \
	src/lib/lx_emul/shadow/mm/percpu.c \
	src/lib/lx_emul/shadow/mm/slab_common.c \
	src/lib/lx_emul/shadow/mm/slub.c \
	src/lib/lx_emul/spec/x86/start.c \
	src/lib/lx_emul/start.c \
	src/lib/lx_emul/virt_to_page.c \
	src/lib/lx_emul/alloc.cc \
	src/lib/lx_emul/clock.cc \
	src/lib/lx_emul/debug.cc \
	src/lib/lx_emul/init.cc \
	src/lib/lx_emul/io_mem.cc \
	src/lib/lx_emul/irq.cc \
	src/lib/lx_emul/log.cc \
	src/lib/lx_emul/page_virt.cc \
	src/lib/lx_emul/task.cc \
	src/lib/lx_emul/time.cc \
	src/lib/lx_kit/console.cc \
	src/lib/lx_kit/env.cc \
	src/lib/lx_kit/init.cc \
	src/lib/lx_kit/memory.cc \
	src/lib/lx_kit/scheduler.cc \
	src/lib/lx_kit/task.cc \
	src/lib/lx_kit/timeout.cc \
	src/lib/lx_kit/spec/x86_64/setjmp.S \
	src/include/spec/x86_64 \
	src/include/spec/x86 \
	src/include/lx_emul \
	src/include/lx_kit \
	src/include/lx_user

MIRROR_FROM_PORT_DIR := \
	$(sort \
		$(addprefix src/linux/, \
			$(shell cat $(PRG_DIR)/source.list) \
			$(shell cd $(PORT_DIR)/src/linux; \
			        find drivers/net/wireguard -type f) \
			$(shell cd $(PORT_DIR)/src/linux; \
			        find kernel -name *.h) \
			$(shell cd $(PORT_DIR)/src/linux; \
			        find lib -name *.h) \
			$(shell cd $(PORT_DIR)/src/linux; \
			        find mm -name *.h) \
			$(shell cd $(PORT_DIR)/src/linux; \
			        find net/core -name *.h) \
			arch/x86/include \
			include \
			crypto/internal.h \
			arch/x86/include/uapi \
			include/uapi \
		) \
	)

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_PORT_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

LICENSE:
	( echo "GNU General Public License version 2, see:"; \
	  echo "https://www.kernel.org/pub/linux/kernel/COPYING" ) > $@
