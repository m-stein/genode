REQUIRES = x86_64

include $(PRG_DIR)/../../target.inc

SRC_S += arch/x86/crypto/poly1305-x86_64-cryptogams.S

arch/x86/crypto/poly1305-x86_64-cryptogams.S:
	perl $(LX_SRC_DIR)/arch/x86/crypto/poly1305-x86_64-cryptogams.pl > $@
