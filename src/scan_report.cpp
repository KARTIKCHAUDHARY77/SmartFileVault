
#include "scan_report.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
    // Escape characters that JSON strings cannot contain directly.
    std::string escapeJsonString(const std::string& text)
    {
        std::string result;
        const char hexDigits[] = "0123456789abcdef";

        for (unsigned char ch : text) {
            switch (ch) {
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
                    if (ch < 0x20) {
                        result += "\\u00";
                        result += hexDigits[(ch >> 4) & 0x0F];
                        result += hexDigits[ch & 0x0F];
                    }
                    else {
                        result += static_cast<char>(ch);
                    }
                    break;
            }
        }

        return result;
    }

    std::string formatFileSize(std::uintmax_t size)
    {
        std::ostringstream output;

        if (size >= 1024ULL * 1024ULL * 1024ULL) {
            output << std::fixed << std::setprecision(2)
                   << static_cast<double>(size)
                      / (1024.0 * 1024.0 * 1024.0)
                   << " GB";
        }
        else if (size >= 1024ULL * 1024ULL) {
            output << std::fixed << std::setprecision(2)
                   << static_cast<double>(size)
                      / (1024.0 * 1024.0)
                   << " MB";
        }
        else if (size >= 1024ULL) {
            output << std::fixed << std::setprecision(2)
                   << static_cast<double>(size) / 1024.0
                   << " KB";
        }
        else {
            output << size << " B";
        }

        return output.str();
    }
}

bool saveScanReport(
    const FileScanResult& result,
    const std::string& outputPath)
{
    std::ofstream file(outputPath, std::ios::binary);

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

    // Include details for files or directories that could not be processed.
    file << "  \"skippedDetails\": [\n";

    for (std::size_t i = 0; i < result.skippedDetails.size(); ++i) {
        const SkippedFile& item = result.skippedDetails[i];

        file << "    {\n";

        file << "      \"filePath\": \""
             << escapeJsonString(item.filePath)
             << "\",\n";

        file << "      \"reason\": \""
             << escapeJsonString(item.reason)
             << "\"\n";

        file << "    }";

        if (i + 1 < result.skippedDetails.size()) {
            file << ",";
        }

        file << "\n";
    }

    file << "  ],\n";

    file << "  \"files\": [\n";

    for (std::size_t i = 0; i < result.files.size(); ++i) {
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

    // Detect disk/write errors instead of reporting success incorrectly.
    file.flush();

    if (!file.good()) {
        return false;
    }

    file.close();

    return !file.fail();
}


#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace
{
    struct JsonSpan { std::size_t begin; std::size_t end; };

    void skipSpace(const std::string& s, std::size_t& p)
    {
        while (p < s.size() && (s[p] == ' ' || s[p] == '\n' ||
               s[p] == '\r' || s[p] == '\t')) ++p;
    }

    bool parseJsonString(const std::string& s, std::size_t& p, std::string& out)
    {
        if (p >= s.size() || s[p] != '"') return false;
        ++p;
        out.clear();
        while (p < s.size()) {
            unsigned char c = static_cast<unsigned char>(s[p++]);
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c != '\\') { out += static_cast<char>(c); continue; }
            if (p >= s.size()) return false;
            char e = s[p++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (p + 4 > s.size()) return false;
                    unsigned int value = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = s[p++];
                        int d = h >= '0' && h <= '9' ? h - '0' :
                                h >= 'a' && h <= 'f' ? h - 'a' + 10 :
                                h >= 'A' && h <= 'F' ? h - 'A' + 10 : -1;
                        if (d < 0) return false;
                        value = (value << 4) | static_cast<unsigned int>(d);
                    }
                    if (value > 0x7f) return false;
                    out += static_cast<char>(value);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool skipJsonValue(const std::string& s, std::size_t& p)
    {
        skipSpace(s, p);
        if (p >= s.size()) return false;
        if (s[p] == '"') {
            std::string ignored;
            return parseJsonString(s, p, ignored);
        }
        if (s[p] == '{' || s[p] == '[') {
            const char open = s[p++];
            const char close = open == '{' ? '}' : ']';
            skipSpace(s, p);
            if (p < s.size() && s[p] == close) { ++p; return true; }
            while (p < s.size()) {
                if (open == '{') {
                    std::string key;
                    if (!parseJsonString(s, p, key)) return false;
                    skipSpace(s, p);
                    if (p >= s.size() || s[p++] != ':') return false;
                }
                if (!skipJsonValue(s, p)) return false;
                skipSpace(s, p);
                if (p < s.size() && s[p] == close) { ++p; return true; }
                if (p >= s.size() || s[p++] != ',') return false;
                skipSpace(s, p);
            }
            return false;
        }
        const std::size_t start = p;
        while (p < s.size() && s[p] != ',' && s[p] != ']' &&
               s[p] != '}' && s[p] != ' ' && s[p] != '\n' &&
               s[p] != '\r' && s[p] != '\t') ++p;
        if (p == start) return false;
        const std::string token = s.substr(start, p - start);
        return token == "true" || token == "false" || token == "null" ||
               std::all_of(token.begin(), token.end(), [](unsigned char c) {
                   return (c >= '0' && c <= '9') || c == '-' || c == '+' ||
                          c == '.' || c == 'e' || c == 'E';
               });
    }

    bool readObjectStringFields(const std::string& s, JsonSpan object,
                                std::string& filePath, std::string& status)
    {
        std::size_t p = object.begin;
        if (p >= s.size() || s[p++] != '{') return false;
        bool gotPath = false, gotStatus = false;
        skipSpace(s, p);
        if (p < s.size() && s[p] == '}') return false;
        while (p < object.end) {
            std::string key;
            if (!parseJsonString(s, p, key)) return false;
            skipSpace(s, p);
            if (p >= object.end || s[p++] != ':') return false;
            skipSpace(s, p);
            if (key == "filePath" || key == "status") {
                std::string value;
                if (!parseJsonString(s, p, value)) return false;
                if (key == "filePath") { filePath = value; gotPath = true; }
                else { status = value; gotStatus = true; }
            } else if (!skipJsonValue(s, p)) return false;
            skipSpace(s, p);
            if (p < object.end && s[p] == '}') {
                ++p;
                break;
            }
            if (p >= object.end || s[p++] != ',') return false;
            skipSpace(s, p);
        }
        return gotPath && gotStatus &&
               (status == "active" || status == "inactive");
    }

    bool findTopLevelFields(const std::string& s,
                            std::map<std::string, JsonSpan>& fields)
    {
        std::size_t p = 0;
        skipSpace(s, p);
        if (p >= s.size() || s[p++] != '{') return false;
        skipSpace(s, p);
        while (p < s.size() && s[p] != '}') {
            std::string key;
            if (!parseJsonString(s, p, key)) return false;
            skipSpace(s, p);
            if (p >= s.size() || s[p++] != ':') return false;
            skipSpace(s, p);
            const std::size_t begin = p;
            if (!skipJsonValue(s, p)) return false;
            fields[key] = {begin, p};
            skipSpace(s, p);
            if (p < s.size() && s[p] == '}') break;
            if (p >= s.size() || s[p++] != ',') return false;
            skipSpace(s, p);
        }
        if (p >= s.size() || s[p++] != '}') return false;
        skipSpace(s, p);
        return p == s.size();
    }

    bool readFileArray(const std::string& s, JsonSpan array,
                       std::vector<JsonSpan>& records)
    {
        std::size_t p = array.begin;
        if (p >= s.size() || s[p++] != '[') return false;
        skipSpace(s, p);
        if (p < array.end && s[p] == ']') return p + 1 == array.end;
        while (p < array.end) {
            const std::size_t begin = p;
            if (!skipJsonValue(s, p)) return false;
            records.push_back({begin, p});
            skipSpace(s, p);
            if (p < array.end && s[p] == ']') return p + 1 == array.end;
            if (p >= array.end || s[p++] != ',') return false;
            skipSpace(s, p);
        }
        return false;
    }

    bool atomicReplaceReport(const std::string& path, const std::string& data)
    {
        struct stat existing {};
        if (::lstat(path.c_str(), &existing) != 0 || !S_ISREG(existing.st_mode))
            return false;
        std::string pattern = path + ".tmp.XXXXXX";
        std::vector<char> temp(pattern.begin(), pattern.end());
        temp.push_back('\0');
        int fd = ::mkstemp(temp.data());
        if (fd < 0) return false;
        const std::string tempPath(temp.data());
        bool ok = (::fchmod(fd, existing.st_mode & 07777) == 0);
        std::size_t written = 0;
        while (ok && written < data.size()) {
            ssize_t count = ::write(fd, data.data() + written, data.size() - written);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) { ok = false; break; }
            written += static_cast<std::size_t>(count);
        }
        if (ok) ok = (::fsync(fd) == 0);
        int saved = errno;
        if (::close(fd) != 0 && ok) { ok = false; saved = errno; }
        if (!ok) {
            ::unlink(tempPath.c_str());
            errno = saved;
            return false;
        }
        if (::rename(tempPath.c_str(), path.c_str()) != 0) {
            saved = errno;
            ::unlink(tempPath.c_str());
            errno = saved;
            return false;
        }
        std::filesystem::path parent = std::filesystem::path(path).parent_path();
        if (parent.empty()) parent = ".";
        int dir = ::open(parent.c_str(), O_RDONLY);
        if (dir < 0) return false;
        ok = (::fsync(dir) == 0);
        saved = errno;
        if (::close(dir) != 0 && ok) { ok = false; saved = errno; }
        if (!ok) errno = saved;
        return ok;
    }
}

bool reconcileScanReportAfterPermanentDelete(
    const std::string& reportPath,
    const ArchiveDeletionResult& deletion,
    std::string& errorMessage)
{
    errorMessage.clear();

    // archiveDeleted alone is insufficient: both vault artifacts must be
    // gone, and the original must actually be gone (or already missing).
    if (!deletion.archiveDeleted ||
        !deletion.compressedRemoved || !deletion.metadataRemoved ||
        (!deletion.originalDeleted && !deletion.originalMissing) ||
        deletion.originalPreserved || deletion.originalPath.empty()) {
        return true;
    }

    std::ifstream input(reportPath, std::ios::binary);
    if (!input.is_open()) { errorMessage = "Could not open scan_report.json."; return false; }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) { errorMessage = "Could not read scan_report.json."; return false; }
    const std::string original = buffer.str();

    std::map<std::string, JsonSpan> fields;
    if (!findTopLevelFields(original, fields)) {
        errorMessage = "scan_report.json is malformed.";
        return false;
    }
    auto filesField = fields.find("files");
    if (filesField == fields.end()) {
        errorMessage = "scan_report.json has no files array.";
        return false;
    }

    std::vector<JsonSpan> records;
    if (!readFileArray(original, filesField->second, records)) {
        errorMessage = "scan_report.json has an invalid files array.";
        return false;
    }

    const std::filesystem::path target =
        std::filesystem::path(deletion.originalPath).lexically_normal();
    std::vector<JsonSpan> retained;
    int active = 0, inactive = 0;
    bool removed = false;
    for (const JsonSpan& record : records) {
        std::string filePath, status;
        if (!readObjectStringFields(original, record, filePath, status)) {
            errorMessage = "scan_report.json contains an invalid file record.";
            return false;
        }
        const std::filesystem::path candidate =
            std::filesystem::path(filePath).lexically_normal();
        if (!removed && candidate == target) {
            removed = true;
            continue;
        }
        retained.push_back(record);
        if (status == "inactive") ++inactive;
        else ++active;
    }

    if (!removed) return true;

    std::ostringstream array;
    array << "[";
    for (std::size_t i = 0; i < retained.size(); ++i) {
        if (i) array << ",";
        array << original.substr(retained[i].begin,
                                 retained[i].end - retained[i].begin);
    }
    array << "]";

    std::vector<std::pair<JsonSpan, std::string>> replacements;
    replacements.push_back({filesField->second, array.str()});
    const std::map<std::string, std::string> counts = {
        {"totalFiles", std::to_string(retained.size())},
        {"inactiveFiles", std::to_string(inactive)},
        {"activeFiles", std::to_string(active)}
    };
    for (const auto& count : counts) {
        auto field = fields.find(count.first);
        if (field == fields.end()) {
            errorMessage = "scan_report.json is missing " + count.first + ".";
            return false;
        }
        replacements.push_back({field->second, count.second});
    }
    std::sort(replacements.begin(), replacements.end(),
        [](const auto& a, const auto& b) { return a.first.begin > b.first.begin; });
    std::string updated = original;
    for (const auto& replacement : replacements) {
        updated.replace(replacement.first.begin,
                        replacement.first.end - replacement.first.begin,
                        replacement.second);
    }

    if (!atomicReplaceReport(reportPath, updated)) {
        errorMessage = "Could not safely persist the updated scan_report.json.";
        return false;
    }
    return true;
}
