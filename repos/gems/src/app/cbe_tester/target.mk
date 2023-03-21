REQUIRES := x86_64

TARGET  := cbe_tester
SRC_CC  += main.cc
SRC_CC  += crypto.cc
SRC_CC  += request_pool.cc
SRC_CC  += sha256_4k_hash.cc
SRC_CC  += trust_anchor.cc
SRC_CC  += block_io.cc
SRC_CC  += meta_tree.cc
SRC_CC  += virtual_block_device.cc
SRC_CC  += superblock_control.cc
SRC_CC  += free_tree.cc
SRC_CC  += vfs_utilities.cc
SRC_CC  += module.cc
SRC_CC  += block_allocator.cc
SRC_CC  += vbd_initializer.cc
SRC_CC  += ft_initializer.cc
SRC_CC  += sb_initializer.cc

INC_DIR := $(PRG_DIR)

LIBS += base
LIBS += vfs
LIBS += cbe
LIBS += cbe_common
LIBS += cbe_cxx
LIBS += cbe_cxx_common
LIBS += libcrypto
LIBS += spark

CC_CXX_WARN_STRICT_CONVERSION =
