#define CHACHA20_IMPLEMENTATION
#include "ChaCha20.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>
#include <random>

namespace fs = std::filesystem;

// SHA-256 implementation (same as in crypto_wrapper.cpp)
void sha256(const std::string& input, uint8_t output[32]) {
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

bool encryptBuffer(std::vector<uint8_t>& buffer, const std::string& keyString) {
    if (buffer.empty() || keyString.empty()) return false;

    key256_t key;
    sha256(keyString, key);

    // Generate random nonce (12 bytes)
    nonce96_t nonce;
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, 255);
    for (int i = 0; i < 12; ++i) {
        nonce[i] = static_cast<uint8_t>(distribution(generator));
    }

    std::vector<uint8_t> encrypted;
    encrypted.reserve(12 + buffer.size());
    encrypted.insert(encrypted.end(), nonce, nonce + 12);
    encrypted.insert(encrypted.end(), buffer.begin(), buffer.end());

    ChaCha20_Ctx ctx;
    ChaCha20_init(&ctx, key, nonce, 0);
    ChaCha20_xor(&ctx, encrypted.data() + 12, buffer.size());

    buffer = encrypted;
    return true;
}

void processFile(const fs::path& path, const std::string& key) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open: " << path << std::endl;
        return;
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (buffer.empty()) {
        std::cout << "Skipped (empty): " << path << std::endl;
        return;
    }

    std::vector<uint8_t> data(buffer.begin(), buffer.end());
    if (!encryptBuffer(data, key)) {
        std::cerr << "Failed to encrypt: " << path << std::endl;
        return;
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to write: " << path << std::endl;
        return;
    }
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    std::cout << "Encrypted: " << path << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) return 1;
    std::string targetDir = argv[1];
    std::string key = argv[2];

    try {
        for (auto& p : fs::recursive_directory_iterator(targetDir)) {
            if (p.is_regular_file() && p.file_size() > 0) {
                auto ext = p.path().extension().string();
                if (ext != ".log" && ext != ".json") {
                    processFile(p.path(), key);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}