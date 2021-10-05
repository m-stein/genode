#
# Linux kernel configuration
#

# kernel fundamentals
LX_ENABLE += TTY SERIAL_EARLYCON SERIAL_OF_PLATFORM PRINTK HAS_IOMEM

# initrd support
LX_ENABLE += BINFMT_ELF BLK_DEV_INITRD

# SoC
#LX_ENABLE += X86_64

# UART device
LX_ENABLE += SERIAL_8250 $(addprefix SERIAL_8250_,16550A_VARIANTS DW CONSOLE)

# network infrastructure
LX_ENABLE += NET NETDEVICES ETHERNET

# network protocols
LX_ENABLE += INET IP_PNP IP_PNP_DHCP

# network driver
LX_ENABLE += PCI E1000 E1000E E100 NET_VENDOR_I825XX IGB IGBVF IXGB IXGBE I40E IGC E1000E_HWTS

## nfs root fs
#LX_ENABLE += NETWORK_FILESYSTEMS NFS_FS NFS_V3 NFS_V4 ROOT_NFS IP_PNP_BOOTP IP_PNP_RARP NFS_USE_KERNEL_DNS NFS_COMMON FILE_LOCKING MULTIUSER

# random number generator
LX_ENABLE += HW_RANDOM ARCH_RANDOM RANDOM_TRUST_CPU RANDOM_TRUST_BOOTLOADER CRYPTO_JITTERENTROPY CRYPTO_ANSI_CPRNG HW_RANDOM_TIMERIOMEM

# wireguard driver
LX_ENABLE += WIREGUARD WIREGUARD_DEBUG

# slim down kernel by removing superfluous drivers
LX_DISABLE += $(addprefix NET_VENDOR_, ALACRITECH AMAZON AQUANTIA ARC BROADCOM \
                                       CADENCE CAVIUM CORTINA EZCHIP GOOGLE \
                                       HISILICON HUAWEI I825XX STMICRO MARVELL \
                                       MICREL MICROCHIP MICROSEMI NATSEMI \
                                       NETRONOME NI 8390 PENSANDO QUALCOMM \
                                       RENESAS ROCKER SAMSUNG SEEQ SOLARFLARE \
                                       SMSC SOCIONEXT NVIDIA OKI MYRI MELLANOX \
                                       MICROSOFT EMULEX DLINK QLOGIC \
                                       PACKET_ENGINES RDC REALTEK SILAN SIS \
                                       SUN SYNOPSYS TEHUTI TI VIA WIZNET XILINX \
                                       3COM ADAPTEC AGERE ALTEON AMD ATHEROS \
                                       BROCADE CHELSIO CISCO DEC NETERION)
LX_DISABLE += WLAN
