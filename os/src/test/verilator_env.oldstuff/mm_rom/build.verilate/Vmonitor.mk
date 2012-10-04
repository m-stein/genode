# Verilated -*- Makefile -*-
# DESCRIPTION: Verilator output: Makefile for building Verilated archive or executable
#
# Execute this makefile from the object directory:
#    make -f Vmonitor.mk

default: Vmonitor

### Constants...
# Perl executable (from $PERL)
PERL = perl
# Path to Verilator kit (from $VERILATOR_ROOT)
VERILATOR_ROOT = /home/hyronimo/LokaleDaten1/GenodeLabs/genode/master/libports/tool/verilator/share/verilator
# Path to SystemPerl kit top (from $SYSTEMPERL)
SYSTEMPERL = 
# Path to SystemPerl kit includes (from $SYSTEMPERL_INCLUDE)
SYSTEMPERL_INCLUDE = 
# SystemC include directory with systemc.h (from $SYSTEMC_INCLUDE)
SYSTEMC_INCLUDE ?= 
# SystemC library directory with libsystemc.a (from $SYSTEMC_LIBDIR)
SYSTEMC_LIBDIR ?= 

### Switches...
# SystemPerl output mode?  0/1 (from --sp)
VM_SP = 0
# SystemC output mode?  0/1 (from --sc)
VM_SC = 0
# SystemPerl or SystemC output mode?  0/1 (from --sp/--sc)
VM_SP_OR_SC = 0
# Deprecated
VM_PCLI = 1
# Deprecated: SystemC architecture to find link library path (from $SYSTEMC_ARCH)
VM_SC_TARGET_ARCH = linux

### Vars...
# Design prefix (from --prefix)
VM_PREFIX = Vmonitor
# Module prefix (from --prefix)
VM_MODPREFIX = Vmonitor
# User CFLAGS (from -CFLAGS on Verilator command line)
VM_USER_CFLAGS = \

# User LDLIBS (from -LDFLAGS on Verilator command line)
VM_USER_LDLIBS = \

# User .cpp files (from .cpp's on Verilator command line)
VM_USER_CLASSES = \
	main \

# User .cpp directories (from .cpp's on Verilator command line)
VM_USER_DIR = \
	build.verilate \


### Default rules...
# Include list of all generated classes
include Vmonitor_classes.mk
# Include global rules
include $(VERILATOR_ROOT)/include/verilated.mk

### Executable rules... (from --exe)
VPATH += $(VM_USER_DIR)

main.o: build.verilate/main.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(OPT_FAST) -c -o $@ $<

### Link rules... (from --exe)
Vmonitor: $(VK_USER_OBJS) $(VK_GLOBAL_OBJS) $(VM_PREFIX)__ALL.a
	@echo "Target ´Vmonitor´ successfully build"


# Verilated -*- Makefile -*-
