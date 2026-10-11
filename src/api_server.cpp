
#include "api_server.h"

#include "application_service.h"
#include "vault_service.h"
#include "vault_report.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>


namespace
{
    // Read one HTTP request, including its complete Content-Length body.
    // Header names are matched without regard to letter case.
    std::string readRequest(int clientSocket)
    {
        constexpr std::size_t maxRequestSize = 1024 * 1024;

        std::string request;
        char buffer[4096];

        while (request.size() <= maxRequestSize) {
            ssize_t bytesRead = recv(
                clientSocket,
                buffer,
                sizeof(buffer),
                0
            );

            if (bytesRead <= 0) {
                break;
            }

            request.append(
                buffer,
                static_cast<std::size_t>(bytesRead)
            );

            if (request.size() > maxRequestSize) {
                return "";
            }

            std::size_t headerEnd =
                request.find("\r\n\r\n");

            if (headerEnd == std::string::npos) {
                continue;
            }

            std::string headerText =
                request.substr(0, headerEnd);

            std::istringstream headerStream(headerText);
            std::string line;

            // The first line is the HTTP request line.
            std::getline(headerStream, line);

            std::optional<std::size_t> contentLength;
            bool invalidHeader = false;

            while (std::getline(headerStream, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                std::size_t colon = line.find(':');

                if (colon == std::string::npos) {
                    continue;
                }

                std::string headerName =
                    line.substr(0, colon);

                std::transform(
                    headerName.begin(),
                    headerName.end(),
                    headerName.begin(),
                    [](unsigned char ch) {
                        return static_cast<char>(
                            std::tolower(ch)
                        );
                    }
                );

                if (headerName != "content-length") {
                    continue;
                }

                std::string value =
                    line.substr(colon + 1);

                std::size_t valueStart =
                    value.find_first_not_of(" \t");

                if (valueStart == std::string::npos ||
                    contentLength.has_value()) {
                    invalidHeader = true;
                    break;
                }

                value =
                    value.substr(valueStart);

                std::size_t valueEnd =
                    value.find_last_not_of(" \t");

                value =
                    value.substr(0, valueEnd + 1);

                try {
                    std::size_t consumed = 0;

                    unsigned long long parsedLength =
                        std::stoull(value, &consumed);

                    if (consumed != value.size() ||
                        parsedLength > maxRequestSize) {
                        invalidHeader = true;
                        break;
                    }

                    contentLength =
                        static_cast<std::size_t>(parsedLength);
                }
                catch (...) {
                    invalidHeader = true;
                    break;
                }
            }

            if (invalidHeader) {
                return "";
            }

            // Requests without Content-Length are treated as
            // having no body. This server does not support
            // chunked request bodies.
            if (!contentLength.has_value()) {
                return request;
            }

            std::size_t bodyStart = headerEnd + 4;

            if (request.size() >=
                bodyStart + contentLength.value()) {
                return request;
            }
        }

        return "";
    }


    std::string getRequestMethod(const std::string& request)
    {
        std::size_t space = request.find(' ');

        if (space == std::string::npos) {
            return "";
        }

        return request.substr(0, space);
    }


    std::string getPath(const std::string& request)
    {
        std::size_t firstSpace =
            request.find(' ');

        if (firstSpace == std::string::npos) {
            return "";
        }

        std::size_t secondSpace =
            request.find(' ', firstSpace + 1);

        if (secondSpace == std::string::npos) {
            return "";
        }

        return request.substr(
            firstSpace + 1,
            secondSpace - firstSpace - 1
        );
    }


    std::string getBody(const std::string& request)
    {
        std::size_t bodyStart =
            request.find("\r\n\r\n");

        if (bodyStart == std::string::npos) {
            return "";
        }

        return request.substr(bodyStart + 4);
    }


    std::string readFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);

        if (!file.is_open()) {
            return "";
        }

        std::ostringstream contents;
        contents << file.rdbuf();

        if (file.bad()) {
            return "";
        }

        return contents.str();
    }


    // Escape strings before placing them into JSON responses.
    std::string jsonEscape(const std::string& text)
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


    // Convert a Unicode code point to UTF-8.
    void appendUtf8(
        std::string& output,
        unsigned int codePoint)
    {
        if (codePoint <= 0x7F) {
            output += static_cast<char>(codePoint);
        }
        else if (codePoint <= 0x7FF) {
            output += static_cast<char>(
                0xC0 | (codePoint >> 6)
            );
            output += static_cast<char>(
                0x80 | (codePoint & 0x3F)
            );
        }
        else if (codePoint <= 0xFFFF) {
            output += static_cast<char>(
                0xE0 | (codePoint >> 12)
            );
            output += static_cast<char>(
                0x80 | ((codePoint >> 6) & 0x3F)
            );
            output += static_cast<char>(
                0x80 | (codePoint & 0x3F)
            );
        }
        else {
            output += static_cast<char>(
                0xF0 | (codePoint >> 18)
            );
            output += static_cast<char>(
                0x80 | ((codePoint >> 12) & 0x3F)
            );
            output += static_cast<char>(
                0x80 | ((codePoint >> 6) & 0x3F)
            );
            output += static_cast<char>(
                0x80 | (codePoint & 0x3F)
            );
        }
    }


    int hexValue(char ch)
    {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }

        if (ch >= 'a' && ch <= 'f') {
            return ch - 'a' + 10;
        }

        if (ch >= 'A' && ch <= 'F') {
            return ch - 'A' + 10;
        }

        return -1;
    }


    // Read a JSON string value and decode its JSON escapes.
    // This is a focused parser for our API's flat request objects,
    // not a complete replacement for a general JSON library.
    std::string getJsonString(
        const std::string& body,
        const std::string& key)
    {
        const std::string search =
            "\"" + key + "\"";

        std::size_t keyPos =
            body.find(search);

        if (keyPos == std::string::npos) {
            return "";
        }

        std::size_t colon =
            body.find(':', keyPos + search.size());

        if (colon == std::string::npos) {
            return "";
        }

        std::size_t pos =
            body.find_first_not_of(
                " \t\r\n",
                colon + 1
            );

        if (pos == std::string::npos ||
            body[pos] != '"') {
            return "";
        }

        ++pos;

        std::string result;

        while (pos < body.size()) {
            char ch = body[pos++];

            if (ch == '"') {
                return result;
            }

            if (ch != '\\') {
                if (static_cast<unsigned char>(ch) < 0x20) {
                    return "";
                }

                result += ch;
                continue;
            }

            if (pos >= body.size()) {
                return "";
            }

            char escaped = body[pos++];

            switch (escaped) {
                case '"':
                    result += '"';
                    break;

                case '\\':
                    result += '\\';
                    break;

                case '/':
                    result += '/';
                    break;

                case 'b':
                    result += '\b';
                    break;

                case 'f':
                    result += '\f';
                    break;

                case 'n':
                    result += '\n';
                    break;

                case 'r':
                    result += '\r';
                    break;

                case 't':
                    result += '\t';
                    break;

                case 'u': {
                    if (pos + 4 > body.size()) {
                        return "";
                    }

                    unsigned int codePoint = 0;

                    for (int i = 0; i < 4; ++i) {
                        int digit = hexValue(body[pos + i]);

                        if (digit < 0) {
                            return "";
                        }

                        codePoint =
                            (codePoint << 4)
                            | static_cast<unsigned int>(digit);
                    }

                    pos += 4;

                    // Decode UTF-16 surrogate pairs.
                    if (codePoint >= 0xD800 &&
                        codePoint <= 0xDBFF) {
                        if (pos + 6 > body.size() ||
                            body[pos] != '\\' ||
                            body[pos + 1] != 'u') {
                            return "";
                        }

                        pos += 2;
                        unsigned int low = 0;

                        for (int i = 0; i < 4; ++i) {
                            int digit = hexValue(body[pos + i]);

                            if (digit < 0) {
                                return "";
                            }

                            low =
                                (low << 4)
                                | static_cast<unsigned int>(digit);
                        }

                        if (low < 0xDC00 || low > 0xDFFF) {
                            return "";
                        }

                        pos += 4;

                        codePoint =
                            0x10000
                            + ((codePoint - 0xD800) << 10)
                            + (low - 0xDC00);
                    }
                    else if (codePoint >= 0xDC00 &&
                             codePoint <= 0xDFFF) {
                        return "";
                    }

                    appendUtf8(result, codePoint);
                    break;
                }

                default:
                    return "";
            }
        }

        // Reject strings without a closing quote.
        return "";
    }


    int getJsonInt(
        const std::string& body,
        const std::string& key)
    {
        const std::string search =
            "\"" + key + "\"";

        std::size_t keyPos =
            body.find(search);

        if (keyPos == std::string::npos) {
            return 0;
        }

        std::size_t colon =
            body.find(':', keyPos + search.size());

        if (colon == std::string::npos) {
            return 0;
        }

        std::size_t start =
            body.find_first_not_of(
                " \t\r\n",
                colon + 1
            );

        if (start == std::string::npos) {
            return 0;
        }

        try {
            std::size_t consumed = 0;

            int value =
                std::stoi(body.substr(start), &consumed);

            if (consumed == 0) {
                return 0;
            }

            std::size_t next =
                body.find_first_not_of(
                    " \t\r\n",
                    start + consumed
                );

            if (next != std::string::npos &&
                body[next] != ',' &&
                body[next] != '}') {
                return 0;
            }

            return value;
        }
        catch (...) {
            return 0;
        }
    }


    std::string makeError(const std::string& message)
    {
        return
            "{\n"
            "  \"success\": false,\n"
            "  \"message\": \"" +
            jsonEscape(message) +
            "\"\n"
            "}";
    }


    std::string makeScanResponse(
        const ApplicationResult& result)
    {
        std::ostringstream json;

        json << "{\n";
        json << "  \"success\": true,\n";

        json << "  \"scannedFiles\": "
             << result.scannedFiles << ",\n";

        json << "  \"scanSkippedFiles\": "
             << result.scanSkippedFiles << ",\n";

        json << "  \"inactiveFiles\": "
             << result.processResult.inactiveFiles << ",\n";

        json << "  \"archivedFiles\": "
             << result.processResult.archivedFiles << ",\n";

        json << "  \"skippedFiles\": "
             << result.processResult.skippedFiles << ",\n";

        json << "  \"spaceSaved\": "
             << result.processResult.spaceSaved << "\n";

        json << "}";

        return json.str();
    }


    // Send all response bytes, including when send() writes partially.
    void sendResponse(
        int clientSocket,
        const std::string& body,
        const std::string& status)
    {
        std::string response;

        response += "HTTP/1.1 " + status + "\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        response += "Access-Control-Allow-Headers: Content-Type\r\n";
        response += "Content-Length: " +
                    std::to_string(body.size()) + "\r\n";
        response += "Connection: close\r\n";
        response += "\r\n";
        response += body;

        std::size_t sent = 0;

        while (sent < response.size()) {
            ssize_t bytesSent = send(
                clientSocket,
                response.data() + sent,
                response.size() - sent,
                0
            );

            if (bytesSent <= 0) {
                break;
            }

            sent += static_cast<std::size_t>(bytesSent);
        }
    }
}


bool startApiServer(
    const std::string& archiveRoot,
    int port)
{
    int serverSocket =
        socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0) {
        std::cout << "Could not create server socket.\n";
        return false;
    }

    int option = 1;

    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &option,
        sizeof(option)
    );

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;

    // The API is intended for the local frontend on this computer.
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddress.sin_port = htons(port);

    if (bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)) < 0) {
        std::cout
            << "Could not bind server to port "
            << port << ".\n";

        close(serverSocket);
        return false;
    }

    if (listen(serverSocket, 10) < 0) {
        std::cout << "Could not start server.\n";
        close(serverSocket);
        return false;
    }

    std::cout << "SmartFileVault API server started.\n";
    std::cout << "http://localhost:" << port << "\n\n";

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);

        int clientSocket = accept(
            serverSocket,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientLength
        );

        if (clientSocket < 0) {
            continue;
        }

        std::string request = readRequest(clientSocket);

        if (request.empty()) {
            sendResponse(
                clientSocket,
                makeError("Invalid or incomplete HTTP request."),
                "400 Bad Request"
            );

            close(clientSocket);
            continue;
        }

        std::string method = getRequestMethod(request);
        std::string path = getPath(request);
        std::string body = getBody(request);

        // Browser CORS preflight.
        if (method == "OPTIONS") {
            sendResponse(clientSocket, "{}", "200 OK");
            close(clientSocket);
            continue;
        }

        // GET dashboard statistics.
        if (method == "GET" && path == "/api/dashboard") {
            std::string reportPath =
                archiveRoot + "/vault_report.json";

            if (saveVaultReport(archiveRoot, reportPath)) {
                std::string json = readFile(reportPath);

                if (!json.empty()) {
                    sendResponse(clientSocket, json, "200 OK");
                }
                else {
                    sendResponse(
                        clientSocket,
                        makeError("Could not read vault report."),
                        "500 Internal Server Error"
                    );
                }
            }
            else {
                sendResponse(
                    clientSocket,
                    makeError("Could not create vault report."),
                    "500 Internal Server Error"
                );
            }
        }

        // GET archives and storage-safety information.
        else if (method == "GET" && path == "/api/archives") {
            std::string reportPath =
                archiveRoot + "/vault_report.json";

            if (saveVaultReport(archiveRoot, reportPath)) {
                std::string json = readFile(reportPath);

                if (!json.empty()) {
                    sendResponse(clientSocket, json, "200 OK");
                }
                else {
                    sendResponse(
                        clientSocket,
                        makeError("Could not read archive report."),
                        "500 Internal Server Error"
                    );
                }
            }
            else {
                sendResponse(
                    clientSocket,
                    makeError("Could not load archives."),
                    "500 Internal Server Error"
                );
            }
        }

        // GET the latest scan report for the frontend.
        else if (method == "GET" && path == "/api/files") {
            std::string reportPath =
                archiveRoot + "/scan_report.json";

            std::string json = readFile(reportPath);

            if (json.empty()) {
                sendResponse(
                    clientSocket,
                    makeError(
                        "No scan report available. Run a scan first."
                    ),
                    "404 Not Found"
                );
            }
            else {
                sendResponse(clientSocket, json, "200 OK");
            }
        }

        // POST scan and archive eligible inactive files.
        else if (method == "POST" && path == "/api/scan") {
            std::string folderPath =
                getJsonString(body, "folderPath");

            int inactiveDays =
                getJsonInt(body, "inactiveDays");

            if (folderPath.empty() || inactiveDays <= 0) {
                sendResponse(
                    clientSocket,
                    makeError("Invalid scan parameters."),
                    "400 Bad Request"
                );
            }
            else {
                ApplicationResult result{};

                bool success = runScanAndArchive(
                    folderPath,
                    inactiveDays,
                    archiveRoot,
                    result
                );

                if (success) {
                    sendResponse(
                        clientSocket,
                        makeScanResponse(result),
                        "200 OK"
                    );
                }
                else {
                    sendResponse(
                        clientSocket,
                        makeError(
                            "Scan and archive operation failed."
                        ),
                        "500 Internal Server Error"
                    );
                }
            }
        }

        // POST restore an archive to the selected destination.
        else if (method == "POST" && path == "/api/restore") {
            std::string archiveId =
                getJsonString(body, "archiveId");

            std::string destination =
                getJsonString(body, "destination");

            if (archiveId.empty() || destination.empty()) {
                sendResponse(
                    clientSocket,
                    makeError("Invalid restore parameters."),
                    "400 Bad Request"
                );
            }
            else {
                std::string restoredPath;

                bool success = restoreFromArchive(
                    archiveRoot,
                    archiveId,
                    destination,
                    restoredPath
                );

                if (success) {
                    std::ostringstream json;

                    json << "{\n";
                    json << "  \"success\": true,\n";
                    json << "  \"restoredPath\": \""
                         << jsonEscape(restoredPath)
                         << "\"\n";
                    json << "}";

                    sendResponse(
                        clientSocket,
                        json.str(),
                        "200 OK"
                    );
                }
                else {
                    sendResponse(
                        clientSocket,
                        makeError("Restore operation failed."),
                        "400 Bad Request"
                    );
                }
            }
        }

        // Unknown endpoint.
        else {
            sendResponse(
                clientSocket,
                makeError("Endpoint not found."),
                "404 Not Found"
            );
        }

        close(clientSocket);
    }

    close(serverSocket);
    return true;
}
