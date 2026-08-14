#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <cstring>

namespace CryptoFormat {

// Magic header prepended to every encrypted asset.
// Allows the engine and packer to distinguish encrypted files from plaintext.
inline constexpr std::array<uint8_t, 4> MAGIC = {'N', 'O', 'V', '1'};
inline constexpr size_t NONCE_SIZE = 12;
inline constexpr size_t HEADER_SIZE = MAGIC.size() + NONCE_SIZE;

inline bool hasMagic(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < MAGIC.size()) return false;
    return std::memcmp(buffer.data(), MAGIC.data(), MAGIC.size()) == 0;
}

inline bool hasMagic(const std::vector<char>& buffer) {
    if (buffer.size() < static_cast<std::ptrdiff_t>(MAGIC.size())) return false;
    return std::memcmp(buffer.data(), MAGIC.data(), MAGIC.size()) == 0;
}

} // namespace CryptoFormat
