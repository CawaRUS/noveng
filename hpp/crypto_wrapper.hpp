#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "crypto_format.hpp"

namespace CryptoWrapper {

// ChaCha20 encryption/decryption
bool encryptBuffer(std::vector<uint8_t>& buffer, const std::string& keyString);
bool decryptBuffer(std::vector<uint8_t>& buffer, const std::string& keyString);

// Derives a separate key for save-file encryption from the asset key.
// The derivation uses SHA-256(assetKey + salt), so saves are not directly
// decryptable with the raw asset key even if it is known.
std::string deriveSaveKey(const std::string& assetKey);

} // namespace CryptoWrapper
