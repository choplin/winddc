# 🖥️ winddc

[![CI](https://github.com/choplin/winddc/actions/workflows/ci.yml/badge.svg)](https://github.com/choplin/winddc/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A Windows CLI tool for controlling monitor brightness, contrast, and input source via DDC/CI.

Windows-native companion to [`m1ddc`](https://github.com/waydabber/m1ddc).

## Install

Download `winddc.exe` from [GitHub Releases](https://github.com/choplin/winddc/releases/latest) and place it in your PATH.

## Usage

```powershell
# List displays (add "detailed" to print MCCS capabilities)
winddc display list

# Basic VCP commands
winddc set luminance 50
winddc get contrast
winddc max volume
winddc chg luminance -10

# Target a specific display
winddc --display 2 set volume 20
```

## NVIDIA-only features

- The repository includes NVIDIA's NvAPI SDK via the `nvapi` submodule and links against `nvapi64.lib`.
- `input-alt` commands (VCP 0xF4, slave 0x50) are sent through raw I²C writes via `NvAPI_I2CWrite`, enabling LG-style alternate input switching (e.g., USB-C codes such as 209/210).
- On systems without a supported NVIDIA GPU, `input-alt` exits with a descriptive error because Dxva2 cannot access the alternate DDC address.

## Building from source

### Requirements

- Windows 10/11 with CMake 3.20+
- Visual Studio 2019/2022 (or Build Tools) with the Windows SDK
- Optional: an NVIDIA GPU + drivers to use `input-alt` via NvAPI

### Build

```powershell
git clone --recurse-submodules <repo-url>
cd winddc

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The resulting executable is `build/winddc.exe`.

## Notes on AMD support

AMD GPUs expose a similar capability through the [AMD Display Library (ADL)](https://github.com/GPUOpen-LibrariesAndSDKs/display-library) API (`ADL_Display_DDCBlockAccess_Set/Get`). If you need `input-alt` on AMD hardware, you'll need access to AMD's SDK and a test machine to implement and verify a helper similar to the NvAPI path. At the moment this repository only ships the NVIDIA integration because we do not have AMD hardware to validate against.

## License

[MIT](LICENSE)
