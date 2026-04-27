#ifndef WINDDC_VCP_H
#define WINDDC_VCP_H

#include <optional>
#include <windows.h>

#include "monitor_store.h"

constexpr BYTE kInputAltCode = 0xF4;
constexpr BYTE kInputAltSourceAddr = 0x50;

std::optional<BYTE> attributeCodeFrom(std::string name);
bool queryVcpValue(HANDLE monitor, BYTE code, DWORD &current, DWORD &maximum);
bool writeVcpValue(const MonitorRecord &monitor, BYTE code, DWORD newValue, uint8_t sourceAddr = kInputAltSourceAddr);

#endif // WINDDC_VCP_H
