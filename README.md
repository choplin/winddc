# win1ddc (Windows port)

This directory hosts a Windows-native CLI companion to `m1ddc`.

## Requirements

- Windows 10/11 with CMake 3.20+.
- Visual Studio 2019/2022 (or Build Tools) with the Windows SDK.
- Optional: an NVIDIA GPU + drivers to use `input-alt` via NvAPI (the project assumes the [`nvapi`](https://github.com/NVIDIA/nvapi) submodule is checked out).

## Building

Clone (or update) with submodules:

```powershell
git clone --recurse-submodules <repo-url>
# or, if already cloned:
git submodule update --init --recursive
```

Then build:

```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The resulting executable is `build/winddc.exe`. Use `--config Release` if you choose a multi-config generator.

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

- The repository includes NVIDIA's NvAPI SDK via the `windows/nvapi` submodule and links against `nvapi64.lib`.
- `input-alt` commands (VCP 0xF4, slave 0x50) are sent through raw I²C writes via `NvAPI_I2CWrite`, enabling LG-style alternate input switching (e.g., USB-C codes such as 209/210).
- On systems without a supported NVIDIA GPU, `input-alt` exits with a descriptive error because Dxva2 cannot access the alternate DDC address.

## Project structure

- `src/main.cpp` – CLI front-end and argument parsing.
- `src/monitor_store.*` – Enumerates DDC-capable monitors and prints capabilities.
- `src/vcp.*` – Attribute lookup plus Get/Set helpers; bridges to NvAPI when needed.
- `src/nvapi_helper.*` – Thin wrapper around NvAPI initialization and raw I²C writes.
- `src/utils.*` – Shared utility functions (string helpers, error formatting, parsing).

## Notes on AMD support

AMD GPUs expose a similar capability through the [AMD Display Library (ADL)](https://github.com/GPUOpen-LibrariesAndSDKs/display-library) API (`ADL_Display_DDCBlockAccess_Set/Get`). If you need `input-alt` on AMD hardware, you'll need access to AMD's SDK and a test machine to implement and verify a helper similar to the NvAPI path. At the moment this repository only ships the NVIDIA integration because we do not have AMD hardware to validate against.
