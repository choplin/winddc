#include "vcp.h"

#include <windows.h>
#include <highlevelmonitorconfigurationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>
#include <iostream>
#include <unordered_map>

#include "nvapi_helper.h"
#include "utils.h"

static const std::unordered_map<std::string, BYTE> kAttrTable = {
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

std::optional<BYTE> attributeCodeFrom(std::string name) {
    name = toLower(std::move(name));
    auto it = kAttrTable.find(name);
    if (it == kAttrTable.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<std::pair<std::string, uint8_t>> allAttributes() {
    // Return full attribute names in display order, excluding single-character aliases
    static const std::vector<std::pair<std::string, uint8_t>> attrs = {
        {"luminance", 0x10},
        {"contrast", 0x12},
        {"input", 0x60},
        {"volume", 0x62},
        {"mute", 0x8D},
        {"standby", 0xD6},
        {"input-alt", kInputAltCode},
        {"red", 0x16},
        {"green", 0x18},
        {"blue", 0x1A},
        {"pbp", 0xE9},
        {"pbp-input", 0xE8},
        {"kvm", 0xE7},
    };
    return attrs;
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
