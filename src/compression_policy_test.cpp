#include <chrono>
#include "compression_policy.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct PolicyTestCase {
    std::string fileName;
    CompressionDecision expected;
};

int main() {
    const std::vector<PolicyTestCase> testCases = {
        {"policy_notes.txt", CompressionDecision::Compress},
        {"policy_code.cpp", CompressionDecision::Compress},
        {"policy_data.csv", CompressionDecision::Compress},
        {"policy_photo.jpg", CompressionDecision::Compress},
        {"policy_image.png", CompressionDecision::Compress},
        {"policy_video.mp4", CompressionDecision::Compress},
        {"policy_music.mp3", CompressionDecision::Compress},
        {"policy_archive.zip", CompressionDecision::Compress},
        {"policy_archive.rar", CompressionDecision::Compress},
        {"policy_archive.7z", CompressionDecision::Compress}
    };

    const auto uniqueId =
    std::chrono::steady_clock::now()
        .time_since_epoch()
        .count();

const fs::path testDirectory =
    fs::temp_directory_path() /
    ("smartfilevault_policy_test_" + std::to_string(uniqueId));

std::error_code error;
fs::create_directory(testDirectory, error);

    if (error) {
        std::cerr << "FAIL: Could not create test directory: "
                  << error.message() << '\n';
        return 1;
    }

    int failures = 0;

    for (const auto& testCase : testCases) {
        const fs::path filePath = testDirectory / testCase.fileName;

        {
            std::ofstream file(filePath, std::ios::binary);
            if (!file) {
                std::cerr << "FAIL: Could not create "
                          << filePath << '\n';
                ++failures;
                continue;
            }
            file << "SmartFileVault compression policy test.\n";
        }

        const CompressionDecision actual =
            getCompressionDecision(filePath.string());

        const bool passed = actual == testCase.expected;

        std::cout << (passed ? "PASS: " : "FAIL: ")
                  << testCase.fileName << " -> "
                  << decisionToString(actual) << '\n';

        if (!passed) {
            ++failures;
        }
    }

    const fs::path missingFile = testDirectory / "does_not_exist.txt";
    const CompressionDecision missingDecision =
        getCompressionDecision(missingFile.string());

    if (missingDecision == CompressionDecision::SkipInvalidFile) {
        std::cout << "PASS: missing file rejected\n";
    } else {
        std::cerr << "FAIL: missing file was not rejected\n";
        ++failures;
    }

    const CompressionDecision directoryDecision =
        getCompressionDecision(testDirectory.string());

    if (directoryDecision == CompressionDecision::SkipInvalidFile) {
        std::cout << "PASS: directory rejected\n";
    } else {
        std::cerr << "FAIL: directory was not rejected\n";
        ++failures;
    }

    std::error_code cleanupError;
    fs::remove_all(testDirectory, cleanupError);

    if (cleanupError) {
        std::cerr << "FAIL: Could not clean test directory: "
                  << cleanupError.message() << '\n';
        ++failures;
    }

    if (failures != 0) {
        std::cerr << "Policy tests failed: " << failures << '\n';
        return 1;
    }

    std::cout << "All compression policy tests passed.\n";
    return 0;
}
