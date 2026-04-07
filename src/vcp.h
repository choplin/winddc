#ifndef WINDDC_VCP_H
#define WINDDC_VCP_H

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

#include "monitor_store.h"

constexpr BYTE kInputAltCode = 0xF4;
constexpr BYTE kInputAltRegister = 0x50;

std::optional<BYTE> attributeCodeFrom(std::string name);
std::vector<std::pair<std::string, uint8_t>> allAttributes();
bool queryVcpValue(HANDLE monitor, BYTE code, DWORD &current, DWORD &maximum);
bool writeVcpValue(const MonitorRecord &monitor, BYTE code, DWORD newValue);

#endif // WINDDC_VCP_H
