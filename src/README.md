# Project Structure

- `main.cpp` – CLI front-end and argument parsing.
- `monitor_store.*` – Enumerates DDC-capable monitors and prints capabilities.
- `vcp.*` – Attribute lookup plus Get/Set helpers; bridges to NvAPI when needed.
- `nvapi_helper.*` – Thin wrapper around NvAPI initialization and raw I²C writes.
- `utils.*` – Shared utility functions (string helpers, error formatting, parsing).
