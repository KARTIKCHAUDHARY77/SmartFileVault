#include "api_server.h"
#include "archive.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "Usage: archive_delete_api_test_server <temporary-root> <port>\n";
        return 2;
    }

    const fs::path root(argv[1]);
    const int port = std::stoi(argv[2]);
    const fs::path original = root / "originals" / "api-delete.txt";
    const fs::path vault = root / "vault";

    std::error_code error;
    fs::create_directories(original.parent_path(), error);
    if (error) {
        std::cerr << "Could not create API test source directory: "
                  << error.message() << '\n';
        return 3;
    }

    {
        std::ofstream file(original, std::ios::binary | std::ios::trunc);
        if (!file) return 4;
        for (int i = 0; i < 1000; ++i) {
            file << "SmartFileVault real API deletion integration fixture.\n";
        }
        file.close();
        if (file.fail()) return 5;
    }

    ArchiveMetadata metadata{};
    if (!createArchive(original.string(), vault.string(), metadata)) {
        std::cerr << "Could not create API deletion archive fixture.\n";
        return 6;
    }

    std::ofstream idFile(root / "archive_id.txt", std::ios::trunc);
    idFile << metadata.archiveId;
    idFile.close();
    if (idFile.fail()) return 7;

    return startApiServer(vault.string(), port) ? 0 : 8;
}
