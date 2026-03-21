#include "utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

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
