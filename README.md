# jcan

can bus diagnostic tool for linux. reverse-engineered usb protocol for vector hardware, socketcan, slcan serial adapters, dbc decoding, logging, transmit scheduling. written in c++23 with dear imgui.

![c++23](https://img.shields.io/badge/c%2B%2B-23-orange)
![imgui](https://img.shields.io/badge/gui-dear%20imgui-blue)
![license](https://img.shields.io/badge/license-MIT-green)

## what is this

a can bus tool i built because the existing options on linux are either ancient, proprietary, or both. or they simply dont exist. supports multiple adapters simultaneously, decodes dbc files in real time, and includes what is (to my knowledge) the first open-source linux driver for vector can hardware.

<img src="img/image.png" alt="showcase" width="90%">

## supported hardware

| adapter                            | interface      | notes                                                  |
| ---------------------------------- | -------------- | ------------------------------------------------------ |
| vector vn1640a                     | usb (libusb)   | reverse-engineered, see above                          |
| kvaser leaf                        | usb (libusb)   | cross-platform, ported from linuxcan gpl source        |
| kvaser mhydra (memorator pro, etc) | usb (libusb)   | hydra protocol, cross-platform, 30 device PIDs         |
| kvaser (any socketcan-supported)   | socketcan      | auto-configures bitrate and link state, sudo elevation |
| canadapter / cantact / candlelight | slcan (serial) | ascii protocol over usb-serial                         |
| any socketcan device               | socketcan      | `can0`, `vcan0`, etc.                                  |

adapter discovery scans serial ports (vid/pid matching for 8 known adapters), socketcan interfaces via netlink, and usb sysfs for vector/peak/kvaser devices. unbound usb devices get helpful hints.

## build

requires cmake 3.25+ and a c++23 compiler (gcc 13+ or clang 17+).

```bash
cmake -B build
cmake --build build -j$(nproc)
```

system dependencies:

- `libusb-1.0` (optional - needed for vector adapter, auto-detected via pkg-config)
- opengl headers + drivers

conditional features:

- `JCAN_ENABLE_SOCKETCAN` - auto-enabled on linux
- `JCAN_ENABLE_VECTOR` - auto-enabled if libusb is found

## run

```bash
./build/jcan_gui          # full gui
./build/jcan_cli          # headless frame dump
```

## for windows

if you are on windows, you must still install drivers:

- candapter FTDI: https://ftdichip.com/wp-content/uploads/2025/03/CDM-v2.12.36.20-WHQL-Certified.zip
- proprietary: install the vendor drivers
- other: https://github.com/daynix/UsbDk/releases

## todo

- [x] make precompiled binaries for all platforms
- [x] ensure compilation on all platforms
- [ ] debug sending messages on vector
- [ ] add sdk / scripting lang to be able to send messages
- [ ] add custom views (to prevent single-use scripts)

## keyboard shortcuts

| key      | action               |
| -------- | -------------------- |
| `Ctrl+O` | open dbc file        |
| `Ctrl+R` | start/stop recording |
| `Ctrl+Q` | quit                 |
