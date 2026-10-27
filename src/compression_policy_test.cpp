#include "compression_policy.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static bool createTestFile(const fs::path& path) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }

    file << "SmartFileVault compression policy test.";
    file.close();
    return !file.fail();
}

static bool checkDecision(const std::string& description,
                          const fs::path& path,
                          CompressionDecision expected) {
    const CompressionDecision actual =
        getCompressionDecision(path.string());

    if (actual != expected) {
        std::cerr << "FAIL: " << description
                  << " | expected " << decisionToString(expected)
                  << ", got " << decisionToString(actual) << '\n';
        return false;
    }

    std::cout << "PASS: " << description << " -> "
              << decisionToString(actual) << '\n';
    return true;
}

int main() {
    const auto uniqueId =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("sfv_policy_test_" + std::to_string(uniqueId));

    std::error_code error;
    if (!fs::create_directory(root, error) || error) {
        std::cerr << "FAIL: Could not create temporary directory\n";
        return 1;
    }

    const std::vector<std::string> eligibleNames = {
        "notes.txt", "code.cpp", "table.csv", "scan.pdf",
        "picture.jpg", "picture.png", "clip.mp4", "clip.mkv",
        "voice.mp3", "voice.wav", "bundle.zip", "bundle.7z",
        "no_extension", "unknown.custom", "UPPER.JPEG"
    };

    bool passed = true;
    for (const std::string& name : eligibleNames) {
        const fs::path path = root / name;
        if (!createTestFile(path)) {
            std::cerr << "FAIL: Could not create fixture "
                      << name << '\n';
            passed = false;
            continue;
        }

        passed &= checkDecision(name, path, CompressionDecision::Compress);
    }

    const fs::path missing = root / "missing.txt";
    passed &= checkDecision("Missing file", missing,
                            CompressionDecision::SkipInvalidFile);

    const fs::path directory = root / "directory.txt";
    error.clear();
    if (!fs::create_directory(directory, error) || error) {
        std::cerr << "FAIL: Could not create directory fixture\n";
        passed = false;
    } else {
        passed &= checkDecision("Directory is invalid", directory,
                                CompressionDecision::SkipInvalidFile);
    }

    error.clear();
    fs::remove_all(root, error);
    if (error) {
        std::cerr << "FAIL: Cleanup failed: "
                  << error.message() << '\n';
        passed = false;
    }

    if (!passed) {
        std::cerr << "COMPRESSION POLICY TESTS FAILED\n";
        return 1;
    }

    std::cout << "ALL COMPRESSION POLICY TESTS PASSED\n";
    return 0;
}
