#include "scan_report.h"

#include <fstream>
#include <iomanip>
#include <sstream>

static std::string escapeJsonString(
    const std::string& text)
{
    std::string result;

    const char hexDigits[] = "0123456789abcdef";

    for (unsigned char ch : text)
    {
        switch (ch)
        {
            case '"':
                result += "\\\"";
                break;

            case '\\':
                result += "\\\\";
                break;

            case '\b':
                result += "\\b";
                break;

            case '\f':
                result += "\\f";
                break;

            case '\n':
                result += "\\n";
                break;

            case '\r':
                result += "\\r";
                break;

            case '\t':
                result += "\\t";
                break;

            default:
                if (ch < 0x20)
                {
                    result += "\\u00";
                    result += hexDigits[(ch >> 4) & 0x0F];
                    result += hexDigits[ch & 0x0F];
                }
                else
                {
                    result += static_cast<char>(ch);
                }
        }
    }

    return result;
}


static std::string formatFileSize(
    std::uintmax_t size)
{
    std::ostringstream output;

    if (size >= 1024ULL * 1024ULL * 1024ULL)
    {
        output << std::fixed << std::setprecision(2)
               << static_cast<double>(size)
                  / (1024.0 * 1024.0 * 1024.0)
               << " GB";
    }
    else if (size >= 1024ULL * 1024ULL)
    {
        output << std::fixed << std::setprecision(2)
               << static_cast<double>(size)
                  / (1024.0 * 1024.0)
               << " MB";
    }
    else if (size >= 1024ULL)
    {
        output << std::fixed << std::setprecision(2)
               << static_cast<double>(size) / 1024.0
               << " KB";
    }
    else
    {
        output << size << " B";
    }

    return output.str();
}


bool saveScanReport(
    const FileScanResult& result,
    const std::string& outputPath)
{
    std::ofstream file(outputPath);

    if (!file.is_open())
    {
        return false;
    }

    int inactiveCount = 0;
    int activeCount = 0;

    for (const auto& item : result.files)
    {
        if (item.inactive)
        {
            inactiveCount++;
        }
        else
        {
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

    for (std::size_t i = 0; i < result.files.size(); i++)
    {
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

        if (i + 1 < result.files.size())
        {
            file << ",";
        }

        file << "\n";
    }

    file << "  ],\n";

    file << "  \"skippedDetails\": [\n";

    for (std::size_t i = 0;
         i < result.skippedDetails.size();
         i++)
    {
        const SkippedFile& item = result.skippedDetails[i];

        file << "    {\n";

        file << "      \"filePath\": \""
             << escapeJsonString(item.filePath)
             << "\",\n";

        file << "      \"reason\": \""
             << escapeJsonString(item.reason)
             << "\"\n";

        file << "    }";

        if (i + 1 < result.skippedDetails.size())
        {
            file << ",";
        }

        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    file.flush();

    if (!file.good())
    {
        return false;
    }

    file.close();

    return !file.fail();
}