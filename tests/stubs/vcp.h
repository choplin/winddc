// Stub vcp.h for testing caps.cpp without Windows dependencies
#ifndef WINDDC_VCP_H
#define WINDDC_VCP_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

std::vector<std::pair<std::string, uint8_t>> allAttributes();

#endif // WINDDC_VCP_H
