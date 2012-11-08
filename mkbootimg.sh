make zImage -j4
../out/host/linux-x86/bin/mkbootimg  --kernel arch/mips/boot/compressed/zImage --ramdisk ramdisk.img --output boot.img
scp boot.img user@192.168.5.99:/home/user/burn/npm801
