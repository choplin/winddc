#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#  define WINVER _WIN32_WINNT
#endif
#ifndef NTDDI_VERSION
#  define NTDDI_VERSION 0x06010000
#endif

#define NOMINMAX
#include <windows.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <highlevelmonitorconfigurationapi.h>

#include <cstdlib>
#include <iostream>
#include <string>

#include "utils.h"

#pragma comment(lib, "Dxva2.lib")
#pragma comment(lib, "nvapi64.lib")

namespace {

struct MonitorRecord {
    PHYSICAL_MONITOR physical{};
    std::wstring deviceName;
};

class MonitorStore {
public:
    MonitorStore() {
        EnumDisplayMonitors(nullptr, nullptr, &MonitorStore::EnumProc, reinterpret_cast<LPARAM>(this));
    }

    ~MonitorStore() {
        for (auto &record : monitors_) {
            if (record.physical.hPhysicalMonitor) {
                DestroyPhysicalMonitor(record.physical.hPhysicalMonitor);
            }
        }
    }

    [[nodiscard]] size_t size() const { return monitors_.size(); }

    [[nodiscard]] const MonitorRecord *get(size_t indexOneBased) const {
        if (indexOneBased == 0 || indexOneBased > monitors_.size()) {
            return nullptr;
        }
        return &monitors_[indexOneBased - 1];
    }

    void list(bool detailed) const {
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
                DWORD len = 0;
                if (GetCapabilitiesStringLength(record.physical.hPhysicalMonitor, &len) && len > 0 && len < 1'048'576) {
                    std::string caps(len, '\0');
                    if (CapabilitiesRequestAndCapabilitiesReply(record.physical.hPhysicalMonitor, caps.data(), len)) {
                        std::cout << "  Capabilities: " << caps.c_str() << "\n";
                    } else {
                        std::cout << "  Capabilities: <unavailable>\n";
                    }
                } else {
                    std::cout << "  Capabilities: <unavailable>\n";
                }
            }
        }
    }

private:
    std::vector<MonitorRecord> monitors_;

    static BOOL CALLBACK EnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) {
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
};

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string narrowString(const std::wstring &value) {
    if (value.empty()) {
        return {};
    }
    int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string narrow(static_cast<size_t>(required - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, narrow.data(), required, nullptr, nullptr);
    return narrow;
}

std::string windowsErrorString(DWORD errorCode) {
    if (errorCode == ERROR_SUCCESS) {
        return {};
    }
    LPSTR buffer = nullptr;
    const DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer),
        0,
        nullptr);
    std::string message = size && buffer ? std::string(buffer, size) : "Unknown error";
    if (buffer) {
        LocalFree(buffer);
    }
    return message;
}

bool parseLong(const char *text, long &value) {
    if (!text || *text == '\0') {
        return false;
    }
    char *end = nullptr;
    long parsed = std::strtol(text, &end, 10);
    if (!end || *end != '\0') {
        return false;
    }
    value = parsed;
    return true;
}

std::string nvapiErrorString(NvAPI_Status status) {
    NvAPI_ShortString buffer;
    if (NvAPI_GetErrorMessage(status, buffer) == NVAPI_OK) {
        return buffer;
    }
    return "Unknown NvAPI error";
}

class NvapiHelper {
public:
    static NvapiHelper &instance() {
        static NvapiHelper helper;
        return helper;
    }

    ~NvapiHelper() {
        if (initialized_) {
            NvAPI_Unload();
        }
    }

    bool sendSetVcpCommand(const MonitorRecord &monitor, BYTE attrCode, DWORD newValue, BYTE registerAddress) const {
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
        i2cInfo.i2cDevAddress = 0x6E; // 0x37 << 1 for write
        NvU8 destAddress = registerAddress;
        i2cInfo.pbI2cRegAddress = &destAddress;
        i2cInfo.regAddrSize = 1;
        std::array<NvU8, 6> payload{};
        payload[0] = 0x84;
        payload[1] = 0x03;
        payload[2] = attrCode;
        payload[3] = static_cast<NvU8>((newValue >> 8) & 0xFF);
        payload[4] = static_cast<NvU8>(newValue & 0xFF);
        payload[5] = static_cast<NvU8>(0x6E ^ destAddress ^ payload[0] ^ payload[1] ^ payload[2] ^ payload[3] ^ payload[4]);
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

private:
    NvapiHelper() = default;

    bool ensureInitialized() const {
        if (initAttempted_) {
            return initialized_;
        }
        initAttempted_ = true;
        const NvAPI_Status status = NvAPI_Initialize();
        if (status == NVAPI_OK) {
            initialized_ = true;
        } else {
            initialized_ = false;
            std::cerr << "NvAPI_Initialize failed: " << nvapiErrorString(status) << "\n";
        }
        return initialized_;
    }

    mutable bool initAttempted_ = false;
    mutable bool initialized_ = false;
};
constexpr BYTE kInputAltCode = 0xF4;
constexpr BYTE kInputAltRegister = 0x50;

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

struct VcpValue {
    DWORD current = 0;
    DWORD maximum = 0;
};

bool queryVcpValue(HANDLE monitor, BYTE code, VcpValue &value) {
    MC_VCP_CODE_TYPE type = MC_MOMENTARY;
    if (!GetVCPFeatureAndVCPFeatureReply(monitor, code, &type, &value.current, &value.maximum)) {
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

void printUsage() {
    std::cout << "Controls DDC/CI capable external displays on Windows via the High-Level Monitor Configuration API.\n\n"
              << "Usage:\n"
              << "  win1ddc display list [detailed]\n"
              << "  win1ddc display <n> (set|get|max|chg) <attribute> [value]\n"
              << "  win1ddc [--display <n>] (set|get|max|chg) <attribute> [value]\n\n"
              << "Examples:\n"
              << "  win1ddc display list detailed\n"
              << "  win1ddc set luminance 65\n"
              << "  win1ddc --display 2 chg volume -5\n";
}

int handleValueCommand(const MonitorRecord &monitor, const std::string &command, int argc, char **argv) {
    if (command == "set") {
        if (argc < 2) {
            std::cerr << "Usage: win1ddc set <attribute> <value>\n";
            return EXIT_FAILURE;
        }
        auto attr = attributeCodeFrom(argv[0]);
        if (!attr) {
            std::cerr << "Unknown attribute: " << argv[0] << "\n";
            return EXIT_FAILURE;
        }
        long parsed = 0;
        if (!parseLong(argv[1], parsed) || parsed < 0) {
            std::cerr << "Invalid value: " << argv[1] << "\n";
            return EXIT_FAILURE;
        }
        if (!writeVcpValue(monitor, *attr, static_cast<DWORD>(parsed))) {
            return EXIT_FAILURE;
        }
        std::cout << "Set " << argv[0] << " to " << parsed << "\n";
        return EXIT_SUCCESS;
    }

    if (command == "get" || command == "max" || command == "chg") {
        if (argc < 1) {
            std::cerr << "Usage: win1ddc " << command << " <attribute>" << (command == "chg" ? " <delta>" : "") << "\n";
            return EXIT_FAILURE;
        }
        auto attr = attributeCodeFrom(argv[0]);
        if (!attr) {
            std::cerr << "Unknown attribute: " << argv[0] << "\n";
            return EXIT_FAILURE;
        }
        VcpValue value{};
        if (!queryVcpValue(monitor.physical.hPhysicalMonitor, *attr, value)) {
            return EXIT_FAILURE;
        }
        if (command == "get") {
            std::cout << argv[0] << " = " << value.current << "\n";
            return EXIT_SUCCESS;
        }
        if (command == "max") {
            std::cout << "max " << argv[0] << " = " << value.maximum << "\n";
            return EXIT_SUCCESS;
        }
        if (command == "chg") {
            if (argc < 2) {
                std::cerr << "Usage: win1ddc chg <attribute> <delta>\n";
                return EXIT_FAILURE;
            }
            long delta = 0;
            if (!parseLong(argv[1], delta)) {
                std::cerr << "Invalid delta: " << argv[1] << "\n";
                return EXIT_FAILURE;
            }
            long updated = static_cast<long>(value.current) + delta;
            if (updated < 0) {
                updated = 0;
            }
            if (updated > static_cast<long>(value.maximum)) {
                updated = static_cast<long>(value.maximum);
            }
            if (!writeVcpValue(monitor, *attr, static_cast<DWORD>(updated))) {
                return EXIT_FAILURE;
            }
            std::cout << argv[0] << " = " << updated << "\n";
            return EXIT_SUCCESS;
        }
    }

    std::cerr << "Unknown command: " << command << "\n";
    printUsage();
    return EXIT_FAILURE;
}

int handleDisplayCommand(const MonitorStore &store, int argc, char **argv) {
    if (argc < 1) {
        printUsage();
        return EXIT_FAILURE;
    }
    std::string subcommand = argv[0];
    if (subcommand == "list") {
        bool detailed = argc >= 2 && std::string(argv[1]) == "detailed";
        store.list(detailed);
        return store.size() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    long index = 0;
    if (!parseLong(subcommand.c_str(), index) || index <= 0) {
        std::cerr << "Invalid display number: " << subcommand << "\n";
        return EXIT_FAILURE;
    }
    const auto *monitor = store.get(static_cast<size_t>(index));
    if (!monitor) {
        std::cerr << "Display #" << index << " not found.\n";
        return EXIT_FAILURE;
    }
    if (argc < 2) {
        std::cerr << "Missing command after display selection.\n";
        printUsage();
        return EXIT_FAILURE;
    }
    return handleValueCommand(*monitor, toLower(argv[1]), argc - 2, argv + 2);
}

int executeCommand(const MonitorStore &store, size_t displayIndex, const std::string &command, int argc, char **argv) {
    const MonitorRecord *monitor = store.get(displayIndex);
    if (!monitor) {
        std::cerr << "Display #" << displayIndex << " not found. Use 'win1ddc display list'.\n";
        return EXIT_FAILURE;
    }
    return handleValueCommand(*monitor, command, argc, argv);
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        printUsage();
        return EXIT_FAILURE;
    }

    MonitorStore store;
    if (store.size() == 0) {
        std::cerr << "No DDC capable displays detected.\n";
        return EXIT_FAILURE;
    }

    std::string firstArg = toLower(argv[1]);

    if (firstArg == "help" || firstArg == "-h" || firstArg == "--help") {
        printUsage();
        return EXIT_SUCCESS;
    }

    if (firstArg == "display") {
        return handleDisplayCommand(store, argc - 2, argv + 2);
    }

    size_t displayIndex = 1;
    int commandArgIndex = 1;
    if (firstArg == "--display") {
        if (argc < 4) {
            std::cerr << "Usage: win1ddc --display <n> <command> ...\n";
            return EXIT_FAILURE;
        }
        long idx = 0;
        if (!parseLong(argv[2], idx) || idx <= 0) {
            std::cerr << "Invalid display number: " << argv[2] << "\n";
            return EXIT_FAILURE;
        }
        displayIndex = static_cast<size_t>(idx);
        commandArgIndex = 3;
        if (commandArgIndex >= argc) {
            std::cerr << "Missing command after --display.\n";
            return EXIT_FAILURE;
        }
        firstArg = toLower(argv[commandArgIndex]);
    }

    return executeCommand(store, displayIndex, firstArg, argc - (commandArgIndex + 1), argv + commandArgIndex + 1);
}
