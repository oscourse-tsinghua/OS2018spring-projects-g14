# README for ucore arm

## Prerequirement

* GNU ARM Toolchain

    Can download from https://launchpad.net/gcc-arm-embedded/+download.

* For `BOARD=goldfishv7`

    Android SDK (recommend r21.0.1) is required.

* For `BOARD=raspberrypi`

    A Raspberry PI (Broadcom BCM2835, ARMv6) is required.

* For other boards

    TODO

## Build

```
cd ucore
make menuconfig ARCH=arm BOARD=<board>
make sfsimg
make
```

All currently available `<board>`:

* `goldfishv7`
* `raspberrypi`
* `pandaboardes`

you may need to modify the *Cross-compiler tool prefix* option based on your own ARM  toolchain, like `arm-none-eabi-`.

## Run

* For `BOARD=goldfishv7`:

    ```
    ./uCore_run -d obj
    ```

    OR

    ```
    emulator-arm -avd <AVD> -kernel obj/kernel.img -no-window -qemu -serial stdio
    ```

* For `BOARD=raspberrypi`:

    1. Write the [Raspbian](https://www.raspberrypi.org/downloads/raspbian/) image to your SD card.
    2. Rename and copy `kernel.img` to the SD card root directory.
    3. Modify or insert `kernel=xxx.img` in `config.txt`.
    4. Plug in the SD card and power on the Raspberry PI.

* For other boards:

    TODO
