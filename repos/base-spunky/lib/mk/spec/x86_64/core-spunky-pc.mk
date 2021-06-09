#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

KERNEL_ADA_DIR = $(BASE_DIR)/../base-spunky/src/core/ada

# add include paths
INC_DIR += $(KERNEL_ADA_DIR)/spec/x86_64
INC_DIR += $(BASE_DIR)/../base-spunky/src/core/spec/x86_64
INC_DIR += $(BASE_DIR)/../base-hw/src/core/spec/x86_64
INC_DIR += $(BASE_DIR)/../base-hw/include/spec/x86_64/

LIBS += syscall-hw

# add assembly sources
SRC_S += spec/x86_64/crt0.s
SRC_S += spec/x86_64/exception_vector.s

# add C++ sources
SRC_CC += kernel/cpu_mp.cc
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += kernel/lock.cc
SRC_CC += spec/x86_64/kernel/thread_exception.cc
SRC_CC += spec/x86_64/platform_support.cc
SRC_CC += spec/x86/platform_services.cc

SRC_CC += spec/x86/io_port_session_component.cc
SRC_CC += spec/x86/io_port_session_support.cc
SRC_CC += spec/x86_64/bios_data_area.cc
SRC_CC += spec/x86_64/cpu.cc
SRC_CC += spec/x86_64/kernel/cpu.cc
SRC_CC += spec/x86_64/kernel/pd.cc
SRC_CC += spec/x86_64/kernel/thread.cc
SRC_CC += spec/x86_64/kernel/thread.cc
SRC_CC += spec/x86_64/platform_support_common.cc

SRC_CC += spec/64bit/memory_map.cc

# add Ada sources
SRC_ADB += timer_device.adb
SRC_ADB += port_io.adb
SRC_ADB += irq_controller_pkg.adb
SRC_ADB += cpp_irq_controller_pkg.adb

vpath spec/64bit/memory_map.cc   $(BASE_DIR)/../base-hw/src/lib/hw
vpath timer_device.adb           $(KERNEL_ADA_DIR)/spec/x86_64
vpath port_io.adb                $(KERNEL_ADA_DIR)/spec/x86_64
vpath irq_controller_pkg.adb     $(KERNEL_ADA_DIR)/spec/x86_64
vpath cpp_irq_controller_pkg.adb $(KERNEL_ADA_DIR)/spec/x86_64

NR_OF_CPUS = 32

# include less specific configuration
include $(BASE_DIR)/../base-spunky/lib/mk/core-spunky.inc
