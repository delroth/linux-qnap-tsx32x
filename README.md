# Linux kernel fork for QNAP TS-x32x

This repository is my work in progress for a recent Linux kernel version
(5.13.3) running on the QNAP TS-x32x series NAS devices. These NAS run on an
Annapurna Labs (now Amazon) Alpine v2 SoC which has only partial upstream
support, though Amazon seems interested in moving more and more parts to the
mainline tree.

## Current status

The following custom hardware is supported:

* Reboot via watchdog
* PCIe
* RTC
* AHCI with MSI-X support (`CONFIG_AHCI_ALPINE`)
* Network via [al_eth-standalone](https://github.com/delroth/al_eth-standalone)
  * Currently only 100Mbps / 1Gbps, and with a few caveats.
* Thermal via [al_thermal-standalone](https://github.com/delroth/al_thermal-standalone)

Still TODO / missing:

* Poweroff
* MTD / NAND
* More `al_eth` work.
* SPI Flash support
* SATA blinkenlights
* A lot of testing

## How to build

```
$ make qnap-tsx82x_defconfig
$ make ARCH=arm64 CROSS_COMPILE=aarch64-unknown-linux-gnu- -j4
```

How to then get the image loaded by the device's u-boot over medium of choice
(TFTP, MMC, ...) is left as an exercise to the reader.
