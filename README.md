# Linux kernel fork for QNAP TS-x32x

This repository is my work in progress for a recent Linux kernel version
(5.5.5) running on the QNAP TS-x32x series NAS devices. These NAS run on an
Annapurna Labs (now Amazon) Alpine v2 SoC which has only partial upstream
support, though Amazon seems interested in moving more and more parts to the
mainline tree.

## Current status

The kernel boots and emits some printk to UART0 via earlycon. It freezes while
probing for psci, probably due to missing DT attributes.

## How to build

```
$ make qnap-tsx82x_defconfig
$ make ARCH=arm64 CROSS_COMPILE=aarch64-unknown-linux-gnu- -j4
```

How to then get the image loaded by the device's u-boot over medium of choice
(TFTP, MMC, ...) is left as an exercise to the reader.
