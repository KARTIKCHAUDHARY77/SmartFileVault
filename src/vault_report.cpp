#include "vault_report.h"

#include "archive.h"
#include "vault_service.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

std::string escapeJson(const std::string& text)
{
    std::string result;

    for (char ch : text) {

        if (ch == '\\') {
            result += "\\\\";
        }
        else if (ch == '"') {
            result += "\\\"";
        }
        else if (ch == '\n') {
            result += "\\n";
        }
        else {
            result += ch;
        }
    }

    return result;
}

std::string formatSize(std::uintmax_t size)
{
    std::ostringstream output;

    if (size >= 1024ULL * 1024ULL * 1024ULL) {

        output << std::fixed << std::setprecision(2)
               << (double)size / (1024.0 * 1024.0 * 1024.0)
               << " GB";
    }
    else if (size >= 1024ULL * 1024ULL) {

        output << std::fixed << std::setprecision(2)
               << (double)size / (1024.0 * 1024.0)
               << " MB";
    }
    else if (size >= 1024ULL) {

        output << std::fixed << std::setprecision(2)
               << (double)size / 1024.0
               << " KB";
    }
    else {

        output << size << " B";
    }

    return output.str();
}

bool saveVaultReport(const std::string& archiveRoot,
                     const std::string& outputPath)
{
    VaultSummary summary;

    if (!getVaultSummary(archiveRoot, summary)) {
        return false;
    }

    std::vector<ArchiveMetadata> archives =
        getAllArchives(archiveRoot);

    std::ofstream file(outputPath);

    if (!file.is_open()) {
        return false;
    }

    file << "{\n";

    file << "  \"archiveCount\": "
         << summary.archiveCount << ",\n";

    file << "  \"originalSize\": "
         << summary.originalSize << ",\n";

    file << "  \"originalSizeText\": \""
         << formatSize(summary.originalSize)
         << "\",\n";

    file << "  \"compressedSize\": "
         << summary.compressedSize << ",\n";

    file << "  \"compressedSizeText\": \""
         << formatSize(summary.compressedSize)
         << "\",\n";

    file << "  \"spaceSaved\": "
         << summary.spaceSaved << ",\n";

    file << "  \"spaceSavedText\": \""
         << formatSize(summary.spaceSaved)
         << "\",\n";

    file << "  \"restoreSafetySpace\": "
         << summary.restoreSafetySpace << ",\n";

    file << "  \"restoreSafetySpaceText\": \""
         << formatSize(summary.restoreSafetySpace)
         << "\",\n";

    file << "  \"freeSpace\": "
         << summary.freeSpace << ",\n";

    file << "  \"freeSpaceText\": \""
         << formatSize(summary.freeSpace)
         << "\",\n";

    file << "  \"archives\": [\n";

    for (std::size_t i = 0; i < archives.size(); i++) {

        const ArchiveMetadata& archive = archives[i];

        file << "    {\n";

        file << "      \"archiveId\": \""
             << escapeJson(archive.archiveId)
             << "\",\n";

        file << "      \"originalName\": \""
             << escapeJson(archive.originalName)
             << "\",\n";

        file << "      \"originalPath\": \""
             << escapeJson(archive.originalPath)
             << "\",\n";

        file << "      \"archivePath\": \""
             << escapeJson(archive.archivePath)
             << "\",\n";

        file << "      \"originalSize\": "
             << archive.originalSize << ",\n";

        file << "      \"originalSizeText\": \""
             << formatSize(archive.originalSize)
             << "\",\n";

        file << "      \"compressedSize\": "
             << archive.compressedSize << ",\n";

        file << "      \"compressedSizeText\": \""
             << formatSize(archive.compressedSize)
             << "\",\n";

        file << "      \"compressionPercentage\": "
             << archive.compressionPercentage << ",\n";

        file << "      \"archivedAt\": "
             << archive.archivedAt << "\n";

        file << "    }";

        if (i + 1 < archives.size()) {
            file << ",";
        }

        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    file.close();

    return true;
}