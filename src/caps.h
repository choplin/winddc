#ifndef WINDDC_CAPS_H
#define WINDDC_CAPS_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

using VcpCapabilities = std::unordered_map<uint8_t, std::vector<uint8_t>>;

VcpCapabilities parseCapabilities(const std::string &capsString);
void printCapabilities(const VcpCapabilities &caps, const std::string &attributeFilter);

#endif // WINDDC_CAPS_H
