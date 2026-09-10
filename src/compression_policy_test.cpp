#include "compression_policy.h"

#include <fstream>
#include <iostream>
#include <string>

void createTestFile(const std::string& fileName) {

    std::ofstream file(fileName);

    file << "SmartFileVault compression policy test.";
}

int main() {

    const std::string testFiles[] = {
        "policy_notes.txt",
        "policy_code.cpp",
        "policy_data.csv",
        "policy_photo.jpg",
        "policy_image.png",
        "policy_video.mp4",
        "policy_music.mp3",
        "policy_archive.zip",
        "policy_archive.rar",
        "policy_archive.7z"
    };

    for (const std::string& file : testFiles) {
        createTestFile(file);
    }

    for (const std::string& file : testFiles) {

        CompressionDecision decision =
            getCompressionDecision(file);

        std::cout << file
                  << " -> "
                  << decisionToString(decision)
                  << '\n';
    }

    for (const std::string& file : testFiles) {
        std::remove(file.c_str());
    }

    return 0;
}