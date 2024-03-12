# SNES Controllers to USB Firmware

## Building

This uses [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel.git)
and the [pico-sdk](https://github.com/raspberrypi/pico-sdk.git).

pico-sdk needs some patches provided by these PRs:
 - [pico-sdk #1530](https://github.com/raspberrypi/pico-sdk/pull/1530)
 - [pico-sdk #1635](https://github.com/raspberrypi/pico-sdk/pull/1635)

And the tinyusb library in the pico-sdk (lib/tinyusb) needs this patch as well:
 - [tinyusb #2474](https://github.com/hathach/tinyusb/pull/2474)

After applying the patches, the build process is similar to a normal pico-sdk
application

```
mkdir build
cd build
cmake ../ -DPICO_SDK_PATH=[path-to-pico-sdk] \
  -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=[Debug|Release|RelWithDebInfo] \
  -DFREERTOS_KERNEL_PATH=[path-to-FreeRTOS-Kernel] -GNinja ninja
```

## Installing

```
# Reset board into programming mode by holding down bootsel
# and pressing the reset button
picotool load -f snes_controllers_to_usb.uf2
picotool reboot # Or just press the reset button
```
