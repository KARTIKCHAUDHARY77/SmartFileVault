
#include "api_server.h"

#include "application_service.h"
#include "vault_service.h"
#include "vault_report.h"

#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
    std::string readRequest(int clientSocket)
    {
        std::string request;
        char buffer[4096];

        while (true) {
            int bytesRead = recv(
                clientSocket,
                buffer,
                sizeof(buffer),
                0
            );

            if (bytesRead <= 0) {
                break;
            }

            request.append(buffer, bytesRead);

            // Keep requests within a reasonable size.
            if (request.size() > 50000) {
                break;
            }

            std::size_t headerEnd =
                request.find("\r\n\r\n");

            // Wait until all HTTP headers have arrived.
            if (headerEnd == std::string::npos) {
                continue;
            }

            std::size_t lengthPos =
                request.find("Content-Length:");

            // GET requests usually have no request body.
            if (lengthPos == std::string::npos) {
                break;
            }

            std::size_t lineEnd =
                request.find("\r\n", lengthPos);

            if (lineEnd == std::string::npos) {
                break;
            }

            std::string lengthText = request.substr(
                lengthPos + 15,
                lineEnd - (lengthPos + 15)
            );

            int contentLength = 0;

            try {
                contentLength = std::stoi(lengthText);
            }
            catch (...) {
                break;
            }

            if (contentLength < 0) {
                break;
            }

            std::size_t bodyStart = headerEnd + 4;

            if (request.size() >=
                bodyStart + static_cast<std::size_t>(contentLength)) {
                break;
            }
        }

        return request;
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
        std::size_t firstSpace = request.find(' ');

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
        std::ifstream file(path);

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


    std::string jsonEscape(const std::string& text)
    {
        std::string result;
        const char hexDigits[] = "0123456789abcdef";

        for (unsigned char ch : text) {
            switch (ch) {
                case '\\':
                    result += "\\\\";
                    break;

                case '"':
                    result += "\\\"";
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


    std::string getJsonString(
        const std::string& body,
        const std::string& key)
    {
        std::string search = "\"" + key + "\"";

        std::size_t keyPos = body.find(search);

        if (keyPos == std::string::npos) {
            return "";
        }

        std::size_t colon = body.find(':', keyPos);

        if (colon == std::string::npos) {
            return "";
        }

        std::size_t firstQuote = body.find('"', colon + 1);

        if (firstQuote == std::string::npos) {
            return "";
        }

        std::size_t secondQuote = body.find('"', firstQuote + 1);

        if (secondQuote == std::string::npos) {
            return "";
        }

        // Simple string extraction for this project's API.
        // This does not decode escaped quotes inside JSON strings.
        return body.substr(
            firstQuote + 1,
            secondQuote - firstQuote - 1
        );
    }


    int getJsonInt(
        const std::string& body,
        const std::string& key)
    {
        std::string search = "\"" + key + "\"";

        std::size_t keyPos = body.find(search);

        if (keyPos == std::string::npos) {
            return 0;
        }

        std::size_t colon = body.find(':', keyPos);

        if (colon == std::string::npos) {
            return 0;
        }

        std::size_t start = body.find_first_not_of(
            " \t\r\n",
            colon + 1
        );

        if (start == std::string::npos) {
            return 0;
        }

        try {
            std::size_t consumed = 0;
            int value = std::stoi(body.substr(start), &consumed);

            // Reject values that are not complete integers.
            std::size_t end = start + consumed;
            std::size_t next = body.find_first_not_of(
                " \t\r\n",
                end
            );

            if (next != std::string::npos && body[next] != ',' &&
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


    std::string makeScanResponse(const ApplicationResult& result)
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
        response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        response += "Connection: close\r\n";
        response += "\r\n";
        response += body;

        // Handle partial sends rather than assuming all bytes
        // are written in one call.
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
    int serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

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
        std::string method = getRequestMethod(request);
        std::string path = getPath(request);
        std::string body = getBody(request);

        // Browser preflight request.
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

        // GET archive list and storage safety information.
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

        // GET results from the latest completed scan.
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

        // POST scan and process inactive files.
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
                        makeError("Scan and archive operation failed."),
                        "500 Internal Server Error"
                    );
                }
            }
        }

        // POST restore an archived file to the chosen directory.
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

        // Unknown route.
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
