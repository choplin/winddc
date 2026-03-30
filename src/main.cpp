#include <windows.h>

#include <cstdlib>
#include <iostream>
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
              << "  winddc [--display <n>] (set|get|max|chg) <attribute> [value]\n\n"
              << "Examples:\n"
              << "  winddc display list detailed\n"
              << "  winddc set luminance 65\n"
              << "  winddc --display 2 chg volume -5\n";
}

int handleValueCommand(const MonitorRecord &monitor, const std::string &command, int argc, char **argv) {
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
        if (!writeVcpValue(monitor, *attr, static_cast<DWORD>(parsed))) {
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
        std::cerr << "Display #" << displayIndex << " not found. Use 'winddc display list'.\n";
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
            std::cerr << "Usage: winddc --display <n> <command> ...\n";
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
