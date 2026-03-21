#include "nvapi_helper.h"

#include <array>
#include <iostream>

#include "nvapi.h"
#include "utils.h"

NvapiHelper &NvapiHelper::instance() {
    static NvapiHelper helper;
    return helper;
}

NvapiHelper::~NvapiHelper() {
    if (initialized_) {
        NvAPI_Unload();
    }
}

bool NvapiHelper::sendSetVcpCommand(const MonitorRecord &monitor, uint8_t attrCode, uint32_t newValue, uint8_t registerAddress) const {
    if (!ensureInitialized()) {
        return false;
    }
    if (monitor.deviceName.empty()) {
        return false;
    }

    const std::string displayName = narrowString(monitor.deviceName);
    if (displayName.empty()) {
        return false;
    }

    NvDisplayHandle displayHandle = nullptr;
    NvAPI_Status status = NvAPI_GetAssociatedNvidiaDisplayHandle(displayName.c_str(), &displayHandle);
    if (status != NVAPI_OK) {
        std::cerr << "NvAPI_GetAssociatedNvidiaDisplayHandle failed: " << nvapiErrorString(status) << "\n";
        return false;
    }

    NvPhysicalGpuHandle gpuHandles[NVAPI_MAX_PHYSICAL_GPUS] = {};
    NvU32 gpuCount = 0;
    status = NvAPI_GetPhysicalGPUsFromDisplay(displayHandle, gpuHandles, &gpuCount);
    if (status != NVAPI_OK || gpuCount == 0) {
        std::cerr << "NvAPI_GetPhysicalGPUsFromDisplay failed: " << nvapiErrorString(status) << "\n";
        return false;
    }

    NvU32 outputId = 0;
    status = NvAPI_GetAssociatedDisplayOutputId(displayHandle, &outputId);
    if (status != NVAPI_OK) {
        std::cerr << "NvAPI_GetAssociatedDisplayOutputId failed: " << nvapiErrorString(status) << "\n";
        return false;
    }

    NV_I2C_INFO i2cInfo = {};
    i2cInfo.version = NV_I2C_INFO_VER;
    i2cInfo.displayMask = outputId;
    i2cInfo.bIsDDCPort = 1;
    i2cInfo.i2cDevAddress = 0x6E;
    uint8_t destAddress = registerAddress;
    i2cInfo.pbI2cRegAddress = &destAddress;
    i2cInfo.regAddrSize = 1;
    std::array<uint8_t, 6> payload{};
    payload[0] = 0x84;
    payload[1] = 0x03;
    payload[2] = attrCode;
    payload[3] = static_cast<uint8_t>((newValue >> 8) & 0xFF);
    payload[4] = static_cast<uint8_t>(newValue & 0xFF);
    payload[5] = static_cast<uint8_t>(0x6E ^ destAddress ^ payload[0] ^ payload[1] ^ payload[2] ^ payload[3] ^ payload[4]);
    i2cInfo.pbData = payload.data();
    i2cInfo.cbSize = static_cast<NvU32>(payload.size());
    i2cInfo.i2cSpeed = NVAPI_I2C_SPEED_DEPRECATED;
    i2cInfo.i2cSpeedKhz = NVAPI_I2C_SPEED_33KHZ;
    i2cInfo.portId = 0;
    i2cInfo.bIsPortIdSet = 0;

    status = NvAPI_I2CWrite(gpuHandles[0], &i2cInfo);
    if (status != NVAPI_OK) {
        std::cerr << "NvAPI_I2CWrite failed: " << nvapiErrorString(status) << "\n";
        return false;
    }
    return true;
}

bool NvapiHelper::ensureInitialized() const {
    if (initAttempted_) {
        return initialized_;
    }
    initAttempted_ = true;
    const NvAPI_Status status = NvAPI_Initialize();
    initialized_ = status == NVAPI_OK;
    if (!initialized_) {
        std::cerr << "NvAPI_Initialize failed: " << nvapiErrorString(status) << "\n";
    }
    return initialized_;
}
