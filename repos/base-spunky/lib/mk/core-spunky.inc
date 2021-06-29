include $(BASE_DIR)/../libports/lib/import/import-spark.mk
include $(BASE_DIR)/../libports/lib/mk/spark_inc_common.inc
include $(BASE_DIR)/../libports/lib/mk/spark_common.inc


###################### kernel Ada sources #####################

SRC_ADB += signal.adb
SRC_ADB += generic_queue.adb
SRC_ADB += generic_double_list.adb
SRC_ADB += cpu_scheduler.adb
SRC_ADB += timer_pkg.adb
SRC_ADB += cpp_timer_pkg.adb
SRC_ADB += log.adb
SRC_ADB += ipc_node.adb
SRC_ADB += cpp.adb
SRC_ADB += cpp_ipc_node.adb
SRC_ADB += cpp_signal.adb
SRC_ADB += cpp_cpu_scheduler.adb

INC_DIR += $(KERNEL_ADA_DIR)

vpath timer_pkg.adb           $(KERNEL_ADA_DIR)
vpath cpp_timer_pkg.adb       $(KERNEL_ADA_DIR)
vpath log.adb                 $(KERNEL_ADA_DIR)
vpath signal.adb              $(KERNEL_ADA_DIR)
vpath generic_queue.adb       $(KERNEL_ADA_DIR)
vpath generic_double_list.adb $(KERNEL_ADA_DIR)
vpath cpu_scheduler.adb       $(KERNEL_ADA_DIR)
vpath ipc_node.adb            $(KERNEL_ADA_DIR)
vpath cpp.adb                 $(KERNEL_ADA_DIR)
vpath cpp_ipc_node.adb        $(KERNEL_ADA_DIR)
vpath cpp_signal.adb          $(KERNEL_ADA_DIR)
vpath cpp_cpu_scheduler.adb   $(KERNEL_ADA_DIR)

CC_ADA_OPT += -gnatec=$(KERNEL_ADA_DIR)/ada.adc

######################### core ########################

#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

LIBS += base-hw-common cxx

# add include paths
INC_DIR += $(BASE_DIR)/../base-spunky/src/core
INC_DIR += $(BASE_DIR)/../base-hw/src/core
INC_DIR += $(BASE_DIR)/../base-hw/src/include
INC_DIR += $(BASE_DIR)/../base-hw/include
INC_DIR += $(BASE_DIR)/../base/src/core/include
INC_DIR += $(BASE_DIR)/../base/src/include

# add C++ sources
SRC_CC += cpu_session_component.cc
SRC_CC += cpu_session_support.cc
SRC_CC += cpu_thread_component.cc
SRC_CC += core_log.cc
SRC_CC += core_log_out.cc
SRC_CC += core_region_map.cc
SRC_CC += core_mem_alloc.cc
SRC_CC += core_rpc_cap_alloc.cc
SRC_CC += dataspace_component.cc
SRC_CC += default_log.cc
SRC_CC += dump_alloc.cc
SRC_CC += kernel_log.cc
SRC_CC += io_mem_session_component.cc
SRC_CC += io_mem_session_support.cc
SRC_CC += irq_session_component.cc
SRC_CC += main.cc
SRC_CC += native_pd_component.cc
SRC_CC += native_utcb.cc
SRC_CC += pd_session_support.cc
SRC_CC += platform.cc
SRC_CC += platform_rom_modules.cc
SRC_CC += platform_pd.cc
SRC_CC += platform_thread.cc
SRC_CC += stack_area.cc
SRC_CC += pd_session_component.cc
SRC_CC += ram_dataspace_support.cc
SRC_CC += region_map_component.cc
SRC_CC += rom_session_component.cc
SRC_CC += trace_session_component.cc
SRC_CC += signal_receiver.cc
SRC_CC += ram_dataspace_factory.cc
SRC_CC += signal_transmitter_noinit.cc
SRC_CC += thread_start.cc
SRC_CC += env.cc
SRC_CC += region_map_support.cc
SRC_CC += pager.cc
SRC_CC += _main.cc
SRC_CC += kernel/cpu.cc
SRC_CC += kernel/init.cc
SRC_CC += kernel/irq.cc
SRC_CC += kernel/kernel.cc
SRC_CC += kernel/object.cc
SRC_CC += kernel/signal_receiver.cc
SRC_CC += kernel/thread.cc
SRC_CC += kernel/timer.cc
SRC_CC += kernel/core_thread.cc
SRC_CC += kernel/log.cc
SRC_CC += init_main_thread.cc
SRC_CC += capability.cc
SRC_CC += stack_area_addr.cc
SRC_CC += heartbeat.cc

# provide Genode version information
include $(BASE_DIR)/src/core/version.inc

# configure multiprocessor mode
NR_OF_CPUS ?= 1
CC_OPT += -Wa,--defsym -Wa,NR_OF_CPUS=$(NR_OF_CPUS) -DNR_OF_CPUS=$(NR_OF_CPUS)

# declare source locations
vpath kernel/signal_receiver.cc $(BASE_DIR)/../base-spunky/src/core
vpath kernel/core_thread.cc     $(BASE_DIR)/../base-spunky/src/core
vpath kernel/log.cc             $(BASE_DIR)/../base-spunky/src/core
vpath kernel/timer.cc           $(BASE_DIR)/../base-spunky/src/core
vpath %                         $(BASE_DIR)/../base-hw/src/core
vpath %                         $(BASE_DIR)/src/core
vpath %                         $(BASE_DIR)/src/lib/startup

CC_ADA_WARN_STRICT :=
