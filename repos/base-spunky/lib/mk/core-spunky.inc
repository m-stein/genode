
LIBPORTS_DIR = /home/lypo/genodelabs/genode/git/repos

#################### import spark ########################

ADA_RTS = $(BUILD_BASE_DIR)/var/libcache/spark

ADA_RTS_SOURCE = $(call select_from_ports,ada-runtime)/ada-runtime/contrib/gcc-8.3.0
ADA_RUNTIME_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/src/minimal
ADA_RUNTIME_LIB_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/src/lib
ADA_RUNTIME_COMMON_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/src/common
ADA_RUNTIME_PLATFORM_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/platform

INC_DIR += $(ADA_RUNTIME_DIR) \
	   $(ADA_RUNTIME_LIB_DIR) \
	   $(ADA_RUNTIME_COMMON_DIR) \
	   $(ADA_RTS_SOURCE) \
	   $(ADA_RUNTIME_PLATFORM_DIR)

# Disable inline concatenation as this requires additinal runtime support
CC_ADA_OPT += -gnatd.c

###################### spark.inc #####################################

all: ada_source_path

ada_source_path: ada_object_path
	$(VERBOSE)echo $(ADA_RTS_SOURCE)   > $(ADA_RTS)/ada_source_path
	$(VERBOSE)echo $(ADA_RUNTIME_DIR) >> $(ADA_RTS)/ada_source_path

ada_object_path:
	$(VERBOSE)mkdir -p $(ADA_RTS)
	$(VERBOSE)echo $(ADA_RTS) > $(ADA_RTS)/ada_object_path

###################### spark.mk #####################################

ADALIB     = $(ADA_RTS)/adalib
ADAINCLUDE = $(ADA_RTS)/adainclude

SRC_ADS += system.ads \
	   s-soflin.ads \
	   s-unstyp.ads \
	   interfac.ads \
	   i-cexten.ads \
	   a-except.ads \
	   ada.ads

SRC_ADB += s-stalib.adb s-secsta.adb s-parame.adb i-c.adb s-arit64.adb s-stoele.adb s-init.adb
CUSTOM_ADA_FLAGS = --RTS=$(ADA_RTS) -c -gnatg -gnatp -gnatpg -gnatn2

# C runtime glue code
SRC_CC += genode.cc
SRC_C += init.c

# Ada packages that implement runtime functionality
SRC_ADS += componolit.ads \
	   componolit-runtime.ads \
	   componolit-runtime-exceptions.ads
SRC_ADB += componolit-runtime-strings.adb \
	   componolit-runtime-debug.adb \
	   componolit-runtime-platform.adb \
	   componolit-runtime-conversions.adb

vpath %.cc  $(ADA_RUNTIME_PLATFORM_DIR)/genode

vpath s-soflin.ads $(ADA_RUNTIME_DIR)
vpath a-except.ads $(ADA_RUNTIME_DIR)

vpath s-secsta.adb $(ADA_RUNTIME_DIR)
vpath s-soflin.adb $(ADA_RUNTIME_DIR)
vpath s-stalib.adb $(ADA_RUNTIME_DIR)
vpath s-parame.adb $(ADA_RUNTIME_DIR)
vpath a-except.adb $(ADA_RUNTIME_DIR)
vpath i-c.adb $(ADA_RUNTIME_DIR)
vpath s-arit64.adb $(ADA_RUNTIME_DIR)
vpath system.ads $(ADA_RUNTIME_DIR)
vpath s-stoele.adb $(ADA_RUNTIME_DIR)
vpath s-init.adb $(ADA_RUNTIME_DIR)

vpath componolit% $(ADA_RUNTIME_LIB_DIR)
vpath init.c $(ADA_RUNTIME_LIB_DIR)
vpath s-init.adb $(ADA_RUNTIME_COMMON_DIR)

vpath %.ads $(ADA_RTS_SOURCE)
vpath %.adb $(ADA_RTS_SOURCE)

###################### kernel Ada sources #####################

SRC_ADB += signal.adb
SRC_ADB += generic_queue.adb
SRC_ADB += generic_double_list.adb
SRC_ADB += cpu_scheduler.adb
SRC_ADB += timer.adb
SRC_ADB += ipc_node.adb
SRC_ADB += cpp.adb
SRC_ADB += cpp_ipc_node.adb
SRC_ADB += cpp_signal.adb
SRC_ADB += cpp_cpu_scheduler.adb

KERNEL_ADA_DIR = $(BASE_DIR)/../base-spunky/src/core/ada

INC_DIR += $(KERNEL_ADA_DIR)

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


######################### Kernel Arch Ada Source #######################

INC_DIR += $(KERNEL_ADA_DIR)/spec/x86_64

ifeq ($(filter-out $(SPECS),64bit),)
INC_DIR += $(KERNEL_ADA_DIR)/spec/64bit
endif # x86_64

ifeq ($(filter-out $(SPECS),32bit),)
INC_DIR += $(KERNEL_ADA_DIR)/spec/32bit
endif # ARM

vpath timer.adb $(KERNEL_ADA_DIR)/spec/x86_64

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
INC_DIR += $(BASE_DIR)/src/core/include
INC_DIR += $(BASE_DIR)/../base-hw/src/include
INC_DIR += $(BASE_DIR)/src/include

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
vpath kernel/core_thread.cc $(BASE_DIR)/../base-spunky/src/core
vpath % $(BASE_DIR)/../base-hw/src/core
vpath % $(BASE_DIR)/src/core
vpath % $(BASE_DIR)/src/lib/startup
