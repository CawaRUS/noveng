#define CHACHA20_IMPLEMENTATION
#include "ChaCha20.h"
#include "crypto_wrapper.hpp"
#include "crypto_format.hpp"
#include <cstring>
#include <random>

namespace CryptoWrapper {

// SHA-256 for key derivation (same as before)
static void sha256(const std::string& input, uint8_t output[32]) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    uint32_t H[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    auto rotr = [](uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); };
    auto ch = [](uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); };
    auto maj = [](uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); };
    auto sigma0 = [&](uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); };
    auto sigma1 = [&](uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); };
    auto gamma0 = [&](uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); };
    auto gamma1 = [&](uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); };

    std::vector<uint8_t> msg(input.begin(), input.end());
    uint64_t bitLen = msg.size() * 8;
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back((bitLen >> (i * 8)) & 0xFF);
    }

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t W[64] = {0};
        for (int i = 0; i < 16; ++i) {
            W[i] = (msg[chunk + i * 4] << 24) | (msg[chunk + i * 4 + 1] << 16) |
                   (msg[chunk + i * 4 + 2] << 8) | msg[chunk + i * 4 + 3];
        }
        for (int i = 16; i < 64; ++i) {
            W[i] = gamma1(W[i - 2]) + W[i - 7] + gamma0(W[i - 15]) + W[i - 16];
        }

        uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
        uint32_t e = H[4], f = H[5], g = H[6], h = H[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t T1 = h + sigma1(e) + ch(e, f, g) + K[i] + W[i];
            uint32_t T2 = sigma0(a) + maj(a, b, c);
            h = g; g = f; f = e; e = d + T1;
            d = c; c = b; b = a; a = T1 + T2;
        }

        H[0] += a; H[1] += b; H[2] += c; H[3] += d;
        H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }

    for (int i = 0; i < 8; ++i) {
        output[i * 4] = (H[i] >> 24) & 0xFF;
        output[i * 4 + 1] = (H[i] >> 16) & 0xFF;
        output[i * 4 + 2] = (H[i] >> 8) & 0xFF;
        output[i * 4 + 3] = H[i] & 0xFF;
    }
}

// ChaCha20 encryption (also works for decryption - it's symmetric)
bool encryptBuffer(std::vector<uint8_t>& buffer, const std::string& keyString) {
    if (buffer.empty() || keyString.empty()) return false;

    // Derive 32-byte key from string
    key256_t key;
    sha256(keyString, key);

    // Generate random nonce (12 bytes)
    nonce96_t nonce;
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, 255);
    for (size_t i = 0; i < CryptoFormat::NONCE_SIZE; ++i) {
        nonce[i] = static_cast<uint8_t>(distribution(generator));
    }

    // Format: [magic][nonce][encrypted payload]
    std::vector<uint8_t> encrypted;
    encrypted.reserve(CryptoFormat::HEADER_SIZE + buffer.size());
    encrypted.insert(encrypted.end(), CryptoFormat::MAGIC.begin(), CryptoFormat::MAGIC.end());
    encrypted.insert(encrypted.end(), nonce, nonce + CryptoFormat::NONCE_SIZE);
    encrypted.insert(encrypted.end(), buffer.begin(), buffer.end());

    // Encrypt the payload part (skip header)
    ChaCha20_Ctx ctx;
    ChaCha20_init(&ctx, key, nonce, 0);
    ChaCha20_xor(&ctx, encrypted.data() + CryptoFormat::HEADER_SIZE, buffer.size());

    buffer = encrypted;
    return true;
}

// ChaCha20 decryption
bool decryptBuffer(std::vector<uint8_t>& buffer, const std::string& keyString) {
    if (buffer.size() < CryptoFormat::HEADER_SIZE || keyString.empty()) return false;

    // Verify magic header; refuse to decrypt files without it to avoid
    // re-decrypting already-processed assets.
    if (!CryptoFormat::hasMagic(buffer)) {
        return false;
    }

    // Derive 32-byte key from string
    key256_t key;
    sha256(keyString, key);

    // Extract nonce from bytes following the magic header
    nonce96_t nonce;
    std::memcpy(nonce, buffer.data() + CryptoFormat::MAGIC.size(), CryptoFormat::NONCE_SIZE);

    // Decrypt the payload part (skip header)
    ChaCha20_Ctx ctx;
    ChaCha20_init(&ctx, key, nonce, 0);
    ChaCha20_xor(&ctx, buffer.data() + CryptoFormat::HEADER_SIZE, buffer.size() - CryptoFormat::HEADER_SIZE);

    // Remove header from buffer
    std::vector<uint8_t> decrypted(buffer.begin() + CryptoFormat::HEADER_SIZE, buffer.end());
    buffer = decrypted;
    return true;
}

std::string deriveSaveKey(const std::string& assetKey) {
    static const std::string salt = "NOVENG_SAVE_SALT_v1";
    std::string input = assetKey + salt;
    uint8_t hash[32];
    sha256(input, hash);

    std::string result;
    result.reserve(32);
    for (int i = 0; i < 32; ++i) {
        result.push_back(static_cast<char>(hash[i]));
    }
    return result;
}

} // namespace CryptoWrapper
