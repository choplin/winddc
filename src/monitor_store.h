#ifndef WINDDC_MONITOR_STORE_H
#define WINDDC_MONITOR_STORE_H

#include <cstddef>
#include <string>
#include <vector>
#include <physicalmonitorenumerationapi.h>

struct MonitorRecord {
    PHYSICAL_MONITOR physical{};
    std::wstring deviceName;
};

class MonitorStore {
public:
    MonitorStore();
    ~MonitorStore();

    [[nodiscard]] size_t size() const;
    [[nodiscard]] const MonitorRecord *get(size_t indexOneBased) const;
    void list(bool detailed) const;
    [[nodiscard]] std::string getCapabilitiesString(size_t indexOneBased) const;

private:
    std::vector<MonitorRecord> monitors_;
    static BOOL CALLBACK EnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam);
};

#endif // WINDDC_MONITOR_STORE_H
