# 🖥️ winddc

[![CI](https://github.com/choplin/winddc/actions/workflows/ci.yml/badge.svg)](https://github.com/choplin/winddc/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A Windows CLI tool for controlling monitors via [DDC/CI](https://en.wikipedia.org/wiki/Display_Data_Channel) — adjust brightness, contrast, volume, input source, and more from the command line.

## Install

Download `winddc.exe` from [GitHub Releases](https://github.com/choplin/winddc/releases/latest) and place it in your PATH.

## Usage

### List displays

```powershell
winddc display list              # List connected displays
winddc display list detailed     # Include MCCS capability details
```

### Get / Set monitor attributes

```powershell
winddc set luminance 50          # Set brightness to 50
winddc get contrast              # Get current contrast value
winddc max volume                # Get maximum volume value
winddc chg luminance -10         # Decrease brightness by 10
```

### Target a specific display

```powershell
winddc --display 2 set volume 20
```

Display number starts from 1. When omitted, display 1 is used.

### Supported attributes

| Attribute | Short | Description |
|-----------|-------|-------------|
| `luminance` | `l` | Brightness |
| `contrast` | `c` | Contrast |
| `volume` | `v` | Volume |
| `mute` | `m` | Mute |
| `input` | `i` | Input source |
| `input-alt` | — | Alternate input source (NVIDIA only, see below) |
| `standby` | `s` | Standby / power mode |
| `red` | `r` | Red gain |
| `green` | `g` | Green gain |
| `blue` | `b` | Blue gain |
| `pbp` | `p` | Picture by Picture |
| `pbp-input` | — | PbP input source |
| `kvm` | `k` | KVM switch |

## NVIDIA-only: `input-alt`

Some LG monitors do not use the standard DDC/CI input-select command (VCP 0x60)
and instead require a vendor-specific VCP code (0xF4) sent via an alternate
DDC/CI source address (0x50 instead of the standard 0x51). This mechanism is
not part of the DDC/CI standard and is not supported by all LG models. See
the [ddcutil wiki](https://github.com/rockowitz/ddcutil/wiki/Switching-input-source-on-LG-monitors)
for details and a list of known compatible monitors.

The standard Windows DDC/CI API (Dxva2) does not support specifying an alternate
source address, so `input-alt` uses NvAPI raw I²C writes to reach the monitor
directly.

**Requires:** NVIDIA GPU with compatible drivers. On systems without a supported
NVIDIA GPU, `input-alt` exits with an error.

### `--i2c-source-addr`

Override the DDC/CI source address byte used by `input-alt`. The default is
`0x50` (the LG side-channel address).

```powershell
winddc --i2c-source-addr 0x50 set input-alt 144
```

This option only affects `input-alt` — other attributes use the standard
Windows DDC/CI API and ignore this setting.

## AMD GPU support

`input-alt` is not supported on AMD GPUs. AMD exposes similar I²C access through the [AMD Display Library (ADL)](https://github.com/GPUOpen-LibrariesAndSDKs/display-library), but this project does not currently include an ADL integration. All other attributes work on any GPU through the standard Windows DDC/CI API (Dxva2).

## Building from source

### Requirements

- Windows 10/11 with CMake 3.20+
- Visual Studio 2019/2022 (or Build Tools) with the Windows SDK
- Optional: an NVIDIA GPU + drivers to use `input-alt` via NvAPI

### Build

```powershell
git clone --recurse-submodules https://github.com/choplin/winddc.git
cd winddc

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The resulting executable is `build/winddc.exe`.

## Acknowledgements

winddc is a Windows-native port of [`m1ddc`](https://github.com/waydabber/m1ddc), a macOS tool for DDC/CI monitor control by [@waydabber](https://github.com/waydabber).

## License

[MIT](LICENSE)
