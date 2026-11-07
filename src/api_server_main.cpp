#include "api_server.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') {
        std::cerr << "ERROR: HOME is not set; cannot choose a default vault location.\n";
        return 1;
    }

    fs::path vaultRoot =
        fs::path(home) / "Library" / "Application Support" /
        "SmartFileVault" / "Vault";

    if (const char* configuredRoot = std::getenv("SFV_VAULT_ROOT")) {
        if (*configuredRoot != '\0') {
            vaultRoot = configuredRoot;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "--help" || argument == "-h") {
            std::cout
                << "SmartFileVault local web application\n\n"
                << "Usage: sfv_api_server [--vault-root PATH]\n\n"
                << "Environment: SFV_VAULT_ROOT overrides the default vault.\n"
                << "The local API listens on 127.0.0.1:8080.\n";
            return 0;
        }

        if (argument == "--vault-root" && i + 1 < argc) {
            vaultRoot = argv[++i];
            continue;
        }

        std::cerr << "ERROR: Unknown or incomplete argument: "
                  << argument << '\n';
        std::cerr << "Use --help for usage.\n";
        return 2;
    }

    std::error_code error;
    fs::create_directories(vaultRoot, error);
    if (error) {
        std::cerr << "ERROR: Could not create vault directory '"
                  << vaultRoot.string() << "': "
                  << error.message() << '\n';
        return 1;
    }

    vaultRoot = fs::absolute(vaultRoot, error).lexically_normal();
    if (error) {
        std::cerr << "ERROR: Could not resolve the vault directory: "
                  << error.message() << '\n';
        return 1;
    }

    std::cout << "Vault directory: " << vaultRoot.string() << '\n';
    std::cout << "Frontend: http://127.0.0.1:8081/\n";
    std::cout << "API:      http://127.0.0.1:8080/\n";
    std::cout << "Press Ctrl+C to stop SmartFileVault.\n\n";

    return startApiServer(vaultRoot.string(), 8080) ? 0 : 1;
}
