#
# \brief  Enable VFPv3-D16-FPU on ARM
# \author Martin Stein
# \date   2014-04-29
#

# enable floating point support in compiler
CC_MARCH += -mfpu=vfpv3-d16 -mfloat-abi=softfp

# include floating-point unit code
REP_INC_DIR += include/arm/vfp
