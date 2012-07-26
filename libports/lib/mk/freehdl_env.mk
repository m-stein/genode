
# add librariy dependencies
LIBS += stdcxx libc libc_log

# avoid warnings from the contrib code
# FIXME should be replaced by patches for the contrib code
CC_OPT += -w

# provide freehdl version to the freehdl contrib-code
FREEHDL_VERSION = 0.0.7
CC_OPT += -DPACKAGE_VERSION='"$(FREEHDL_VERSION)"'

# ensure that freehdl contrib-code uses 'unistd.h'
CC_OPT += -DHAVE_UNISTD_H=1

# add include paths
FREEHDL_CONTRIB_DIR = $(REP_DIR)/contrib/freehdl-$(FREEHDL_VERSION)
INC_DIR += $(FREEHDL_CONTRIB_DIR)

# add c++ source-files from contrib/freehdl-X.X.X/kernel
SRC_CC += main.cc \
          fhdl_stream.cc \
          name_stack.cc \
          kernel_class.cc \
          register.cc \
          process_base.cc \
          handle.cc \
          error.cc \
          stack_trace.cc \
          db.cc \
          global_event_queue.cc \
          reader_info.cc \
          sig_info.cc \
          resolver_process.cc \
          map_list.cc \
          signal_source_list_array.cc \
          dump.cc \
          acl.cc \
          persistent_dump.cc \
          sigacl_list.cc \
          driver_info.cc \
          wait_info.cc \
          persistent_cdfg_dump.cc

# add c++ source-files from contrib/freehdl-X.X.X/std
SRC_CC += standard.cc \
          internal_textio.cc \
          memory.cc \
          vhdl_types.cc

# supply missing implementations of 'stdcxx' library
# FIXME might be a bug of the 'stdcxx' library
SRC_CC += stdcxx_supplement.cc

# declare source paths
vpath % $(REP_DIR)/src/lib/freehdl
vpath % $(FREEHDL_CONTRIB_DIR)/std
vpath % $(FREEHDL_CONTRIB_DIR)/kernel

