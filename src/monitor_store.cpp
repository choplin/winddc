#include "monitor_store.h"

#include <windows.h>
#include <highlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <iostream>
#include <string>
#include <vector>

MonitorStore::MonitorStore() {
    EnumDisplayMonitors(nullptr, nullptr, &MonitorStore::EnumProc, reinterpret_cast<LPARAM>(this));
}

MonitorStore::~MonitorStore() {
    for (auto &record : monitors_) {
        if (record.physical.hPhysicalMonitor) {
            DestroyPhysicalMonitor(record.physical.hPhysicalMonitor);
        }
    }
}

size_t MonitorStore::size() const {
    return monitors_.size();
}

const MonitorRecord *MonitorStore::get(size_t indexOneBased) const {
    if (indexOneBased == 0 || indexOneBased > monitors_.size()) {
        return nullptr;
    }
    return &monitors_[indexOneBased - 1];
}

void MonitorStore::list(bool detailed) const {
    if (monitors_.empty()) {
        std::cout << "No DDC capable external displays were found.\n";
        return;
    }
    for (size_t i = 0; i < monitors_.size(); ++i) {
        const auto &record = monitors_[i];
        std::wcout << L"Display " << (i + 1) << L": " << record.physical.szPhysicalMonitorDescription << L"\n";
        if (!record.deviceName.empty()) {
            std::wcout << L"  Device: " << record.deviceName << L"\n";
        }
        if (detailed) {
            auto caps = getCapabilitiesString(i + 1);
            if (!caps.empty()) {
                std::cout << "  Capabilities: " << caps << "\n";
            } else {
                std::cout << "  Capabilities: <unavailable>\n";
            }
        }
    }
}

std::string MonitorStore::getCapabilitiesString(size_t indexOneBased) const {
    const auto *record = get(indexOneBased);
    if (!record) {
        return "";
    }
    DWORD len = 0;
    if (!GetCapabilitiesStringLength(record->physical.hPhysicalMonitor, &len) || len == 0 || len >= 1'048'576) {
        return "";
    }
    std::string caps(len, '\0');
    if (!CapabilitiesRequestAndCapabilitiesReply(record->physical.hPhysicalMonitor, caps.data(), len)) {
        return "";
    }
    return std::string(caps.c_str()); // trim trailing null
}

BOOL CALLBACK MonitorStore::EnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) {
    auto *store = reinterpret_cast<MonitorStore *>(lParam);
    if (!store) {
        return FALSE;
    }

    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    std::wstring deviceName;
    if (GetMonitorInfoW(hMonitor, &info)) {
        deviceName = info.szDevice;
    }

    DWORD monitorCount = 0;
    if (!GetNumberOfPhysicalMonitorsFromHMONITOR(hMonitor, &monitorCount) || monitorCount == 0) {
        return TRUE;
    }
    std::vector<PHYSICAL_MONITOR> physicals(monitorCount);
    if (!GetPhysicalMonitorsFromHMONITOR(hMonitor, monitorCount, physicals.data())) {
        return TRUE;
    }
    for (auto &physical : physicals) {
        store->monitors_.push_back({physical, deviceName});
    }
    return TRUE;
}
