// Copyright 2026 AiCode Contributors
// SPDX-License-Identifier: Apache-2.0

#include "common/string_utils.h"

#ifdef _WIN32
#include <windows.h>
#include <iostream>
#else
#include <iostream>
#endif

namespace aicode {

bool IsUtf8(const std::string& input) {
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c < 0x80) continue;  // ASCII
        if (c >= 0xC2 && c <= 0xDF) {
            if (++i >= input.size() || (input[i] & 0xC0) != 0x80) { return false; }
        } else if (c >= 0xE0 && c <= 0xEF) {
            if (i + 2 >= input.size() || (input[++i] & 0xC0) != 0x80 || (input[++i] & 0xC0) != 0x80) { return false; }
        } else if (c >= 0xF0 && c <= 0xF4) {
            if (i + 3 >= input.size() || (input[++i] & 0xC0) != 0x80 || (input[++i] & 0xC0) != 0x80 || (input[++i] & 0xC0) != 0x80) { return false; }
        } else { return false; }
    }
    return true;
}

std::string ConvertToUtf8(const std::string& input) {
    // Check if already valid UTF-8
    if (IsUtf8(input)) {
        return input;
    }

#ifdef _WIN32
    // Try to convert from system default encoding (e.g., GBK/GB2312) to UTF-8
    int wide_len = MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, nullptr, 0);
    if (wide_len > 0) {
        std::wstring wide(wide_len, 0);
        MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, &wide[0], wide_len);
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, &wide[0], -1, nullptr, 0, nullptr, nullptr);
        if (utf8_len > 0) {
            std::string utf8(utf8_len, 0);
            WideCharToMultiByte(CP_UTF8, 0, &wide[0], -1, &utf8[0], utf8_len, nullptr, nullptr);
            utf8.pop_back();  // Remove null terminator
            return utf8;
        }
    }
#endif

    // Fallback: return as-is
    return input;
}

std::string ReadLine() {
#ifdef _WIN32
    // Windows: Read console input as wide chars and convert to UTF-8
    HANDLE h_input = GetStdHandle(STD_INPUT_HANDLE);
    std::string line;

    if (h_input != INVALID_HANDLE_VALUE) {
        wchar_t wbuf[4096];
        DWORD chars_read = 0;
        if (ReadConsoleW(h_input, wbuf, sizeof(wbuf) / sizeof(wchar_t) - 1, &chars_read, nullptr)) {
            // Remove trailing \r\n
            while (chars_read > 0 && (wbuf[chars_read - 1] == L'\r' || wbuf[chars_read - 1] == L'\n')) {
                chars_read--;
            }
            if (chars_read > 0) {
                std::wstring wline(wbuf, chars_read);
                // Convert UTF-16 to UTF-8 using helper
                int size = WideCharToMultiByte(CP_UTF8, 0, wline.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (size > 0) {
                    line.resize(size - 1);
                    WideCharToMultiByte(CP_UTF8, 0, wline.c_str(), -1, &line[0], size, nullptr, nullptr);
                }
            }
        } else {
            // Fallback: read as bytes and convert from system encoding (e.g., GBK)
            std::getline(std::cin, line);
            line = ConvertToUtf8(line);
        }
    } else {
        std::getline(std::cin, line);
        line = ConvertToUtf8(line);
    }
    return line;
#else
    // POSIX: Assume terminal is already UTF-8 (modern standard)
    std::string line;
    std::getline(std::cin, line);
    return line;
#endif
}

}  // namespace aicode
