#ifndef WINDDC_NVAPI_HELPER_H
#define WINDDC_NVAPI_HELPER_H

#include <cstdint>

#include "monitor_store.h"

class NvapiHelper {
public:
    static NvapiHelper &instance();
    ~NvapiHelper();

    bool sendSetVcpCommand(const MonitorRecord &monitor, uint8_t attrCode, uint32_t newValue, uint8_t registerAddress) const;

private:
    NvapiHelper() = default;
    bool ensureInitialized() const;

    mutable bool initAttempted_ = false;
    mutable bool initialized_ = false;
};

#endif // WINDDC_NVAPI_HELPER_H
