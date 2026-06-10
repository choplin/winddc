#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include "monitor_store.h"
#include "utils.h"
#include "vcp.h"

namespace {

void printUsage() {
    std::cout << "Controls DDC/CI capable external displays on Windows via Dxva2.\n\n"
              << "Usage:\n"
              << "  winddc display list [detailed]\n"
              << "  winddc display <n> (set|get|max|chg) <attribute> [value]\n"
              << "  winddc [--display <n>] [--i2c-source-addr <hex>] (set|get|max|chg) <attribute> [value]\n\n"
              << "Options:\n"
              << "  --display <n>              Select display by number\n"
              << "  --i2c-source-addr <hex>    DDC/CI source address for input-alt (default: 0x50)\n\n"
              << "Examples:\n"
              << "  winddc display list detailed\n"
              << "  winddc set luminance 65\n"
              << "  winddc --display 2 chg volume -5\n"
              << "  winddc --i2c-source-addr 0x51 set input-alt 144\n";
}

std::optional<uint8_t> parseHexByte(const char *text) {
    if (!text || *text == '\0') {
        return std::nullopt;
    }
    char *end = nullptr;
    unsigned long parsed = std::strtoul(text, &end, 0);
    if (!end || *end != '\0' || parsed > 0xFF) {
        return std::nullopt;
    }
    return static_cast<uint8_t>(parsed);
}

int handleValueCommand(const MonitorRecord &monitor, const std::string &command, int argc, char **argv, uint8_t sourceAddr = kInputAltSourceAddr) {
    if (command == "set") {
        if (argc < 2) {
            std::cerr << "Usage: winddc set <attribute> <value>\n";
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
        if (!writeVcpValue(monitor, *attr, static_cast<DWORD>(parsed), sourceAddr)) {
            return EXIT_FAILURE;
        }
        std::cout << "Set " << argv[0] << " to " << parsed << "\n";
        return EXIT_SUCCESS;
    }

    if (command == "get" || command == "max" || command == "chg") {
        if (argc < 1) {
            std::cerr << "Usage: winddc " << command << " <attribute>" << (command == "chg" ? " <delta>" : "") << "\n";
            return EXIT_FAILURE;
        }
        auto attr = attributeCodeFrom(argv[0]);
        if (!attr) {
            std::cerr << "Unknown attribute: " << argv[0] << "\n";
            return EXIT_FAILURE;
        }
        DWORD current = 0;
        DWORD maximum = 0;
        if (!queryVcpValue(monitor.physical.hPhysicalMonitor, *attr, current, maximum)) {
            return EXIT_FAILURE;
        }
        if (command == "get") {
            std::cout << argv[0] << " = " << current << "\n";
            return EXIT_SUCCESS;
        }
        if (command == "max") {
            std::cout << "max " << argv[0] << " = " << maximum << "\n";
            return EXIT_SUCCESS;
        }
        if (command == "chg") {
            if (argc < 2) {
                std::cerr << "Usage: winddc chg <attribute> <delta>\n";
                return EXIT_FAILURE;
            }
            long delta = 0;
            if (!parseLong(argv[1], delta)) {
                std::cerr << "Invalid delta: " << argv[1] << "\n";
                return EXIT_FAILURE;
            }
            long updated = static_cast<long>(current) + delta;
            if (updated < 0) {
                updated = 0;
            }
            if (updated > static_cast<long>(maximum)) {
                updated = static_cast<long>(maximum);
            }
            if (!writeVcpValue(monitor, *attr, static_cast<DWORD>(updated), sourceAddr)) {
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

int executeCommand(const MonitorStore &store, size_t displayIndex, const std::string &command, int argc, char **argv, uint8_t sourceAddr = kInputAltSourceAddr) {
    const MonitorRecord *monitor = store.get(displayIndex);
    if (!monitor) {
        std::cerr << "Display #" << displayIndex << " not found. Use 'winddc display list'.\n";
        return EXIT_FAILURE;
    }
    return handleValueCommand(*monitor, command, argc, argv, sourceAddr);
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
    uint8_t sourceAddr = kInputAltSourceAddr;
    int commandArgIndex = 1;

    while (commandArgIndex < argc) {
        std::string arg = toLower(argv[commandArgIndex]);
        if (arg == "--display") {
            if (commandArgIndex + 1 >= argc) {
                std::cerr << "Usage: winddc --display <n> <command> ...\n";
                return EXIT_FAILURE;
            }
            long idx = 0;
            if (!parseLong(argv[commandArgIndex + 1], idx) || idx <= 0) {
                std::cerr << "Invalid display number: " << argv[commandArgIndex + 1] << "\n";
                return EXIT_FAILURE;
            }
            displayIndex = static_cast<size_t>(idx);
            commandArgIndex += 2;
        } else if (arg == "--i2c-source-addr") {
            if (commandArgIndex + 1 >= argc) {
                std::cerr << "Usage: winddc --i2c-source-addr <hex> <command> ...\n";
                return EXIT_FAILURE;
            }
            auto parsed = parseHexByte(argv[commandArgIndex + 1]);
            if (!parsed) {
                std::cerr << "Invalid I2C source address: " << argv[commandArgIndex + 1] << "\n";
                return EXIT_FAILURE;
            }
            sourceAddr = *parsed;
            // TODO: error when --i2c-source-addr is used with non-input-alt attributes
            commandArgIndex += 2;
        } else {
            break;
        }
    }

    if (commandArgIndex >= argc) {
        std::cerr << "Missing command.\n";
        printUsage();
        return EXIT_FAILURE;
    }
    firstArg = toLower(argv[commandArgIndex]);

    return executeCommand(store, displayIndex, firstArg, argc - (commandArgIndex + 1), argv + commandArgIndex + 1, sourceAddr);
}
