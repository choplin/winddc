#include "vcp.h"

#include <windows.h>
#include <highlevelmonitorconfigurationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>
#include <iostream>
#include <unordered_map>

#include "nvapi_helper.h"
#include "utils.h"

std::optional<BYTE> attributeCodeFrom(std::string name) {
    static const std::unordered_map<std::string, BYTE> attrTable = {
        {"luminance", BYTE{0x10}}, {"l", BYTE{0x10}},
        {"contrast", BYTE{0x12}}, {"c", BYTE{0x12}},
        {"volume", BYTE{0x62}}, {"v", BYTE{0x62}},
        {"mute", BYTE{0x8D}}, {"m", BYTE{0x8D}},
        {"input", BYTE{0x60}}, {"i", BYTE{0x60}},
        {"input-alt", kInputAltCode},
        {"standby", BYTE{0xD6}}, {"s", BYTE{0xD6}},
        {"red", BYTE{0x16}}, {"r", BYTE{0x16}},
        {"green", BYTE{0x18}}, {"g", BYTE{0x18}},
        {"blue", BYTE{0x1A}}, {"b", BYTE{0x1A}},
        {"pbp", BYTE{0xE9}}, {"p", BYTE{0xE9}},
        {"pbp-input", BYTE{0xE8}},
        {"kvm", BYTE{0xE7}}, {"k", BYTE{0xE7}}
    };
    name = toLower(std::move(name));
    auto it = attrTable.find(name);
    if (it == attrTable.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool queryVcpValue(HANDLE monitor, BYTE code, DWORD &current, DWORD &maximum) {
    MC_VCP_CODE_TYPE type = MC_MOMENTARY;
    if (!GetVCPFeatureAndVCPFeatureReply(monitor, code, &type, &current, &maximum)) {
        const auto err = windowsErrorString(GetLastError());
        std::cerr << "Failed to read VCP feature (0x" << std::hex << static_cast<int>(code) << std::dec << "): " << err;
        return false;
    }
    return true;
}

bool writeVcpValue(const MonitorRecord &monitor, BYTE code, DWORD newValue) {
    if (code == kInputAltCode) {
        if (!NvapiHelper::instance().sendSetVcpCommand(monitor, code, newValue, kInputAltRegister)) {
            std::cerr << "input-alt requires an NVIDIA GPU with NvAPI support; command failed.\n";
            return false;
        }
        return true;
    }
    if (!SetVCPFeature(monitor.physical.hPhysicalMonitor, code, newValue)) {
        const auto err = windowsErrorString(GetLastError());
        std::cerr << "Failed to set VCP feature (0x" << std::hex << static_cast<int>(code) << std::dec << "): " << err;
        return false;
    }
    return true;
}
