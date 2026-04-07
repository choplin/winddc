#include "caps.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <iomanip>

#include <vcp.h>

static const std::unordered_map<uint8_t, std::string> kInputSourceNames = {
    {0x01, "VGA-1"}, {0x02, "VGA-2"}, {0x03, "DVI-1"}, {0x04, "DVI-2"},
    {0x05, "Composite-1"}, {0x06, "Composite-2"},
    {0x07, "S-Video-1"}, {0x08, "S-Video-2"},
    {0x09, "Tuner-1"}, {0x0A, "Tuner-2"}, {0x0B, "Tuner-3"},
    {0x0C, "Component-1"}, {0x0D, "Component-2"}, {0x0E, "Component-3"},
    {0x0F, "DisplayPort-1"}, {0x10, "DisplayPort-2"},
    {0x11, "HDMI-1"}, {0x12, "HDMI-2"},
};

static std::string findVcpSection(const std::string &capsString) {
    // Find "vcp(" case-insensitively
    std::string lower = capsString;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto pos = lower.find("vcp(");
    if (pos == std::string::npos) {
        return "";
    }

    // Track parentheses nesting from the '(' after "vcp"
    size_t start = pos + 4; // right after "vcp("
    int depth = 1;
    size_t i = start;
    while (i < capsString.size() && depth > 0) {
        if (capsString[i] == '(') {
            ++depth;
        } else if (capsString[i] == ')') {
            --depth;
        }
        ++i;
    }
    if (depth != 0) {
        return "";
    }
    // i points one past the closing ')'; content is [start, i-1)
    return capsString.substr(start, i - 1 - start);
}

static uint8_t parseHexByte(const std::string &token) {
    unsigned long val = std::strtoul(token.c_str(), nullptr, 16);
    return static_cast<uint8_t>(val);
}

VcpCapabilities parseCapabilities(const std::string &capsString) {
    VcpCapabilities result;
    std::string vcpContent = findVcpSection(capsString);
    if (vcpContent.empty()) {
        return result;
    }

    size_t pos = 0;
    size_t len = vcpContent.size();

    auto skipSpaces = [&]() {
        while (pos < len && std::isspace(static_cast<unsigned char>(vcpContent[pos]))) {
            ++pos;
        }
    };

    auto readHexToken = [&]() -> std::string {
        skipSpaces();
        std::string token;
        while (pos < len && std::isxdigit(static_cast<unsigned char>(vcpContent[pos]))) {
            token += vcpContent[pos];
            ++pos;
        }
        return token;
    };

    while (pos < len) {
        skipSpaces();
        if (pos >= len) break;

        std::string codeToken = readHexToken();
        if (codeToken.empty()) {
            // Skip unexpected characters
            ++pos;
            continue;
        }

        uint8_t vcpCode = parseHexByte(codeToken);
        std::vector<uint8_t> values;

        skipSpaces();
        if (pos < len && vcpContent[pos] == '(') {
            // Discrete values inside parentheses
            ++pos; // skip '('
            while (pos < len && vcpContent[pos] != ')') {
                std::string valToken = readHexToken();
                if (!valToken.empty()) {
                    values.push_back(parseHexByte(valToken));
                } else {
                    ++pos; // skip unexpected char
                }
            }
            if (pos < len) {
                ++pos; // skip ')'
            }
        }

        result[vcpCode] = std::move(values);
    }

    return result;
}

static void printAttribute(const std::string &name, uint8_t code,
                            const VcpCapabilities &caps) {
    auto it = caps.find(code);
    std::cout << name << " (VCP 0x" << std::uppercase << std::hex
              << std::setfill('0') << std::setw(2)
              << static_cast<int>(code) << std::dec << "): ";

    if (it == caps.end()) {
        std::cout << "not listed in capabilities\n";
        return;
    }

    const auto &values = it->second;
    if (values.empty()) {
        std::cout << "continuous\n";
        return;
    }

    // VCP 0x60 (input) gets human-readable names
    if (code == 0x60) {
        std::cout << "\n";
        for (auto v : values) {
            std::cout << "  " << static_cast<int>(v);
            auto nameIt = kInputSourceNames.find(v);
            if (nameIt != kInputSourceNames.end()) {
                std::cout << " = " << nameIt->second;
            }
            std::cout << "\n";
        }
        return;
    }

    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << static_cast<int>(values[i]);
    }
    std::cout << "\n";
}

void printCapabilities(const VcpCapabilities &caps, const std::string &attributeFilter) {
    auto attrs = allAttributes();

    if (!attributeFilter.empty()) {
        // Find the matching attribute
        bool found = false;
        for (const auto &[name, code] : attrs) {
            if (name == attributeFilter) {
                printAttribute(name, code, caps);
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "Unknown attribute: " << attributeFilter << "\n";
        }
        return;
    }

    // Print all known attributes that appear in capabilities
    for (const auto &[name, code] : attrs) {
        auto it = caps.find(code);
        if (it != caps.end()) {
            printAttribute(name, code, caps);
        }
    }
}
