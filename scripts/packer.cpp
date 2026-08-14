#include "crypto_wrapper.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

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

    // Avoid double-encrypting files that already carry the engine's magic header.
    if (CryptoFormat::hasMagic(buffer)) {
        std::cout << "Skipped (already encrypted): " << path << std::endl;
        return;
    }

    std::vector<uint8_t> data(buffer.begin(), buffer.end());
    if (!CryptoWrapper::encryptBuffer(data, key)) {
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
