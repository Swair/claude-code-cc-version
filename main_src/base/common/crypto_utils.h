// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <openssl/evp.h>
#include <random>
#include <sstream>
#include <string>

namespace prosophor {

/// Sha256Hex - Compute SHA-256 digest and return as uppercase hex string
inline std::string Sha256Hex(const std::string& input) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int len = 0;
    EVP_Digest(input.data(), input.size(), digest.data(), &len, EVP_sha256(), nullptr);
    std::ostringstream oss;
    for (unsigned int i = 0; i < len; ++i) {
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(digest[i]);
    }
    return oss.str();
}

/// GenerateUuid - Generate a UUID v4 string (32 hex chars, no dashes)
inline std::string GenerateUuid() {
    std::array<uint8_t, 16> bytes{};
    std::random_device rd;
    for (int i = 0; i < 16; i += 4) {
        uint32_t r = rd();
        std::memcpy(bytes.data() + i, &r, 4);
    }
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    std::ostringstream oss;
    for (int i = 0; i < 16; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

}  // namespace prosophor
