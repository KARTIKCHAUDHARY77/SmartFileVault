#include "scan_report.h"

#include <fstream>
#include <iomanip>
#include <sstream>

std::string escapeJsonString(const std::string& text)
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

std::string formatFileSize(std::uintmax_t size)
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

bool saveScanReport(const FileScanResult& result,
                    const std::string& outputPath)
{
    std::ofstream file(outputPath);

    if (!file.is_open()) {
        return false;
    }

    int inactiveCount = 0;
    int activeCount = 0;

    for (const auto& item : result.files) {
        if (item.inactive) {
            inactiveCount++;
        }
        else {
            activeCount++;
        }
    }

    file << "{\n";

    file << "  \"totalFiles\": "
         << result.files.size() << ",\n";

    file << "  \"inactiveFiles\": "
         << inactiveCount << ",\n";

    file << "  \"activeFiles\": "
         << activeCount << ",\n";

    file << "  \"skippedFiles\": "
         << result.skippedFiles << ",\n";

    file << "  \"files\": [\n";

    for (std::size_t i = 0; i < result.files.size(); i++) {

        const FileMetadata& item = result.files[i];

        file << "    {\n";

        file << "      \"fileName\": \""
             << escapeJsonString(item.fileName)
             << "\",\n";

        file << "      \"filePath\": \""
             << escapeJsonString(item.filePath)
             << "\",\n";

        file << "      \"extension\": \""
             << escapeJsonString(item.extension)
             << "\",\n";

        file << "      \"size\": "
             << item.size << ",\n";

        file << "      \"sizeText\": \""
             << formatFileSize(item.size)
             << "\",\n";

        file << "      \"lastModified\": "
             << item.lastModified << ",\n";

        file << "      \"status\": \""
             << (item.inactive ? "inactive" : "active")
             << "\"\n";

        file << "    }";

        if (i + 1 < result.files.size()) {
            file << ",";
        }

        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    file.close();

    return true;
}