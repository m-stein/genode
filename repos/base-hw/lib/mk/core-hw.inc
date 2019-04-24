
LIBPORTS_DIR = /home/lypo/genodelabs/genode/git/repos

#################### import spark ########################

ADA_RTS = $(BUILD_BASE_DIR)/var/libcache/spark

ADA_RTS_SOURCE = $(call select_from_ports,ada-runtime)/ada-runtime/contrib/gcc-6.3.0
ADA_RUNTIME_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/src/minimal
ADA_RUNTIME_LIB_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/src/lib
ADA_RUNTIME_COMMON_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/src/common
ADA_RUNTIME_PLATFORM_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/platform

INC_DIR += $(ADA_RUNTIME_DIR)
INC_DIR += $(ADA_RUNTIME_LIB_DIR)
INC_DIR += $(ADA_RUNTIME_COMMON_DIR)
INC_DIR += $(ADA_RTS_SOURCE)

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
	   s-imgint.ads \
	   s-stoele.ads \
	   s-unstyp.ads \
	   interfac.ads \
	   i-cexten.ads \
	   a-except.ads \
	   gnat.ads \
	   ada.ads \
	   ada_exceptions.ads

SRC_ADB += g-io.adb s-stalib.adb s-secsta.adb s-parame.adb i-c.adb s-arit64.adb
CUSTOM_ADA_FLAGS = --RTS=$(ADA_RTS) -c -gnatg -gnatp -gnatpg -gnatn2

# C runtime glue code
SRC_CC += genode.cc
SRC_C += init.c

# Ada packages that implement runtime functionality
SRC_ADB += ss_utils.adb string_utils.adb platform.adb s-init.adb

vpath genode.cc  $(ADA_RUNTIME_PLATFORM_DIR)

vpath s-soflin.ads $(ADA_RUNTIME_DIR)
vpath a-except.ads $(ADA_RUNTIME_DIR)

vpath s-secsta.adb $(ADA_RUNTIME_DIR)
vpath s-soflin.adb $(ADA_RUNTIME_DIR)
vpath s-stalib.adb $(ADA_RUNTIME_DIR)
vpath s-parame.adb $(ADA_RUNTIME_DIR)
vpath a-except.adb $(ADA_RUNTIME_DIR)
vpath i-c.adb $(ADA_RUNTIME_DIR)

vpath %.ads $(ADA_RTS_SOURCE)
vpath %.adb $(ADA_RTS_SOURCE)

vpath platform.% $(ADA_RUNTIME_LIB_DIR)
vpath string_utils.% $(ADA_RUNTIME_LIB_DIR)
vpath ss_utils.% $(ADA_RUNTIME_LIB_DIR)
vpath ada_exceptions.ads $(ADA_RUNTIME_LIB_DIR)
vpath init.c $(ADA_RUNTIME_LIB_DIR)
vpath s-init.adb $(ADA_RUNTIME_COMMON_DIR)

###################### kernel SPARK sources #####################

SRC_ADB += ipc_node.adb
SRC_ADB += cpp_ipc_node.adb

INC_DIR += /home/lypo/genodelabs/genode/git/repos/base-hw/src/core/spark

vpath ipc_node.adb /home/lypo/genodelabs/genode/git/repos/base-hw/src/core/spark
vpath cpp_ipc_node.adb /home/lypo/genodelabs/genode/git/repos/base-hw/src/core/spark

######################### core ########################

#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

LIBS += base-hw-common cxx

# add include paths
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
SRC_CC += platform_genode.cc
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
SRC_CC += kernel/cpu_scheduler.cc
SRC_CC += kernel/double_list.cc
SRC_CC += kernel/init.cc
SRC_CC += kernel/ipc_node.cc
SRC_CC += kernel/irq.cc
SRC_CC += kernel/kernel.cc
SRC_CC += kernel/object.cc
SRC_CC += kernel/signal_receiver.cc
SRC_CC += kernel/thread.cc
SRC_CC += kernel/timer.cc
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
vpath % $(BASE_DIR)/../base-hw/src/core
vpath % $(BASE_DIR)/src/core
vpath % $(BASE_DIR)/src/lib/startup
