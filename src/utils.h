#ifndef WINDDC_UTILS_H
#define WINDDC_UTILS_H

#include <string>
#include <windows.h>

#include "nvapi.h"

std::string toLower(std::string value);
std::string narrowString(const std::wstring &value);
std::string windowsErrorString(DWORD errorCode);
bool parseLong(const char *text, long &value);
std::string nvapiErrorString(NvAPI_Status status);

#endif // WINDDC_UTILS_H
