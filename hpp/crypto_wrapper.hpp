#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace CryptoWrapper {

// ChaCha20 encryption/decryption
bool encryptBuffer(std::vector<uint8_t>& buffer, const std::string& keyString);
bool decryptBuffer(std::vector<uint8_t>& buffer, const std::string& keyString);

} // namespace CryptoWrapper
