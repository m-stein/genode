
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

###################### machinery #####################

SRC_ADB += machinery.adb

INC_DIR += /home/lypo/genodelabs/genode/git/repos/base-nova/src/core

vpath machinery.adb /home/lypo/genodelabs/genode/git/repos/base-nova/src/core
vpath machinery.ads /home/lypo/genodelabs/genode/git/repos/base-nova/src/core



######################### core ########################


LIBS         += base-nova-common cxx

GEN_CORE_DIR = $(BASE_DIR)/src/core

SRC_CC      += stack_area.cc \
               core_mem_alloc.cc \
               core_log.cc \
               core_log_out.cc \
               core_region_map.cc \
               core_rpc_cap_alloc.cc \
               cpu_session_component.cc \
               cpu_session_support.cc \
               cpu_thread_component.cc \
               dataspace_component.cc \
               default_log.cc \
               dump_alloc.cc \
               io_mem_session_component.cc \
               io_mem_session_support.cc \
               io_port_session_component.cc \
               io_port_session_support.cc \
               ipc_pager.cc \
               irq_session_component.cc \
               main.cc \
               pager.cc \
               native_cpu_component.cc \
               native_pd_component.cc \
               pd_session_support.cc \
               rpc_cap_factory.cc \
               ram_dataspace_factory.cc \
               platform_genode.cc \
               platform_rom_modules.cc \
               platform_pd.cc \
               platform_services.cc \
               platform_thread.cc \
               pd_session_component.cc \
               ram_dataspace_support.cc \
               region_map_component.cc \
               rom_session_component.cc \
               thread_start.cc \
               bios_data_area.cc \
               trace_session_component.cc \
               signal_transmitter_noinit.cc \
               signal_receiver.cc \
               vm_session_component.cc \
               vm_session_common.cc \
               heartbeat.cc

INC_DIR     += $(REP_DIR)/src/core/include \
               $(REP_DIR)/src/include \
               $(BASE_DIR)/src/include \
               $(GEN_CORE_DIR)/include

include $(GEN_CORE_DIR)/version.inc

vpath main.cc                      $(GEN_CORE_DIR)
vpath pd_session_component.cc      $(GEN_CORE_DIR)
vpath rom_session_component.cc     $(GEN_CORE_DIR)
vpath core_log.cc                  $(GEN_CORE_DIR)
vpath cpu_session_component.cc     $(GEN_CORE_DIR)
vpath cpu_session_support.cc       $(GEN_CORE_DIR)
vpath cpu_thread_component.cc      $(GEN_CORE_DIR)
vpath pd_upgrade_ram_quota.cc      $(GEN_CORE_DIR)
vpath region_map_component.cc      $(GEN_CORE_DIR)
vpath trace_session_component.cc   $(GEN_CORE_DIR)
vpath signal_transmitter_noinit.cc $(GEN_CORE_DIR)
vpath signal_receiver.cc           $(GEN_CORE_DIR)
vpath io_port_session_component.cc $(GEN_CORE_DIR)/spec/x86
vpath io_port_session_support.cc   $(GEN_CORE_DIR)/spec/x86
vpath io_mem_session_component.cc  $(GEN_CORE_DIR)
vpath io_mem_session_support.cc    $(GEN_CORE_DIR)
vpath ram_dataspace_factory.cc     $(GEN_CORE_DIR)
vpath dataspace_component.cc       $(GEN_CORE_DIR)
vpath core_mem_alloc.cc            $(GEN_CORE_DIR)
vpath default_log.cc               $(GEN_CORE_DIR)
vpath dump_alloc.cc                $(GEN_CORE_DIR)
vpath platform_rom_modules.cc      $(GEN_CORE_DIR)
vpath stack_area.cc                $(GEN_CORE_DIR)
vpath heartbeat.cc                 $(GEN_CORE_DIR)
vpath vm_session_common.cc         $(GEN_CORE_DIR)
vpath %.cc                         $(REP_DIR)/src/core

