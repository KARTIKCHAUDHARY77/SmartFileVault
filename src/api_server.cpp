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

        int bytesRead;

        while ((bytesRead =
                    recv(clientSocket,
                         buffer,
                         sizeof(buffer),
                         0)) > 0) {

            request.append(buffer, bytesRead);

            // Most of our requests are small.
            if (request.size() > 50000) {
                break;
            }

            // For our simple API, stop after receiving
            // the request body.
            if (request.find("\r\n\r\n") !=
                std::string::npos) {

                std::size_t contentLengthPos =
                    request.find("Content-Length:");

                if (contentLengthPos == std::string::npos) {
                    break;
                }

                std::size_t lineEnd =
                    request.find(
                        "\r\n",
                        contentLengthPos
                    );

                if (lineEnd != std::string::npos) {

                    std::string lengthText =
                        request.substr(
                            contentLengthPos + 15,
                            lineEnd -
                            (contentLengthPos + 15)
                        );

                    int contentLength =
                        std::stoi(lengthText);

                    std::size_t bodyStart =
                        request.find("\r\n\r\n") + 4;

                    if (request.size() >=
                        bodyStart + contentLength) {
                        break;
                    }
                }
            }
        }

        return request;
    }


    std::string getRequestMethod(
        const std::string& request)
    {
        std::size_t space =
            request.find(' ');

        if (space == std::string::npos) {
            return "";
        }

        return request.substr(0, space);
    }


    std::string getPath(
        const std::string& request)
    {
        std::size_t firstSpace =
            request.find(' ');

        if (firstSpace == std::string::npos) {
            return "";
        }

        std::size_t secondSpace =
            request.find(
                ' ',
                firstSpace + 1
            );

        if (secondSpace == std::string::npos) {
            return "";
        }

        return request.substr(
            firstSpace + 1,
            secondSpace - firstSpace - 1
        );
    }


    std::string getBody(
        const std::string& request)
    {
        std::size_t bodyStart =
            request.find("\r\n\r\n");

        if (bodyStart == std::string::npos) {
            return "";
        }

        return request.substr(bodyStart + 4);
    }


    std::string readFile(
        const std::string& path)
    {
        std::ifstream file(path);

        if (!file.is_open()) {
            return "";
        }

        std::ostringstream contents;
        contents << file.rdbuf();

        return contents.str();
    }


    std::string jsonEscape(
        const std::string& text)
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
            else if (ch == '\r') {
                result += "\\r";
            }
            else {
                result += ch;
            }
        }

        return result;
    }


    std::string getJsonString(
        const std::string& body,
        const std::string& key)
    {
        std::string search =
            "\"" + key + "\"";

        std::size_t keyPos =
            body.find(search);

        if (keyPos == std::string::npos) {
            return "";
        }

        std::size_t colon =
            body.find(':', keyPos);

        if (colon == std::string::npos) {
            return "";
        }

        std::size_t firstQuote =
            body.find('"', colon);

        if (firstQuote == std::string::npos) {
            return "";
        }

        std::size_t secondQuote =
            body.find('"', firstQuote + 1);

        if (secondQuote == std::string::npos) {
            return "";
        }

        return body.substr(
            firstQuote + 1,
            secondQuote - firstQuote - 1
        );
    }


    int getJsonInt(
        const std::string& body,
        const std::string& key)
    {
        std::string search =
            "\"" + key + "\"";

        std::size_t keyPos =
            body.find(search);

        if (keyPos == std::string::npos) {
            return 0;
        }

        std::size_t colon =
            body.find(':', keyPos);

        if (colon == std::string::npos) {
            return 0;
        }

        std::size_t start =
            body.find_first_of(
                "0123456789",
                colon + 1
            );

        if (start == std::string::npos) {
            return 0;
        }

        std::size_t end =
            body.find_first_not_of(
                "0123456789",
                start
            );

        try {
            return std::stoi(
                body.substr(
                    start,
                    end - start
                )
            );
        }
        catch (...) {
            return 0;
        }
    }


    std::string makeError(
        const std::string& message)
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
             << result.processResult.spaceSaved
             << "\n";

        json << "}";

        return json.str();
    }


    void sendResponse(
        int clientSocket,
        const std::string& body,
        const std::string& status)
    {
        std::string response;

        response +=
            "HTTP/1.1 " + status + "\r\n";

        response +=
            "Content-Type: application/json\r\n";

        response +=
            "Access-Control-Allow-Origin: *\r\n";

        response +=
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";

        response +=
            "Access-Control-Allow-Headers: Content-Type\r\n";

        response +=
            "Content-Length: " +
            std::to_string(body.size()) +
            "\r\n";

        response +=
            "Connection: close\r\n";

        response += "\r\n";
        response += body;

        send(
            clientSocket,
            response.c_str(),
            response.size(),
            0
        );
    }
}


bool startApiServer(
    const std::string& archiveRoot,
    int port)
{
    int serverSocket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (serverSocket < 0) {

        std::cout
            << "Could not create server socket.\n";

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

    serverAddress.sin_family =
        AF_INET;

    serverAddress.sin_addr.s_addr =
        INADDR_ANY;

    serverAddress.sin_port =
        htons(port);

    if (bind(
            serverSocket,
            (sockaddr*)&serverAddress,
            sizeof(serverAddress)) < 0) {

        std::cout
            << "Could not bind server to port "
            << port << ".\n";

        close(serverSocket);

        return false;
    }

    if (listen(serverSocket, 10) < 0) {

        std::cout
            << "Could not start server.\n";

        close(serverSocket);

        return false;
    }

    std::cout
        << "SmartFileVault API server started.\n";

    std::cout
        << "http://localhost:"
        << port
        << "\n\n";


    while (true) {

        sockaddr_in clientAddress{};
        socklen_t clientLength =
            sizeof(clientAddress);

        int clientSocket =
            accept(
                serverSocket,
                (sockaddr*)&clientAddress,
                &clientLength
            );

        if (clientSocket < 0) {
            continue;
        }

        std::string request =
            readRequest(clientSocket);

        std::string method =
            getRequestMethod(request);

        std::string path =
            getPath(request);

        std::string body =
            getBody(request);


        // Browser preflight request
        if (method == "OPTIONS") {

            sendResponse(
                clientSocket,
                "{}",
                "200 OK"
            );

            close(clientSocket);
            continue;
        }


        // GET dashboard
        if (method == "GET" &&
            path == "/api/dashboard") {

            std::string reportPath =
                archiveRoot +
                "/vault_report.json";

            if (saveVaultReport(
                    archiveRoot,
                    reportPath)) {

                std::string json =
                    readFile(reportPath);

                sendResponse(
                    clientSocket,
                    json,
                    "200 OK"
                );
            }
            else {

                sendResponse(
                    clientSocket,
                    makeError(
                        "Could not create vault report."
                    ),
                    "500 Internal Server Error"
                );
            }
        }


        // GET archives
        else if (method == "GET" &&
                 path == "/api/archives") {

            std::string reportPath =
                archiveRoot +
                "/vault_report.json";

            if (saveVaultReport(
                    archiveRoot,
                    reportPath)) {

                std::string json =
                    readFile(reportPath);

                sendResponse(
                    clientSocket,
                    json,
                    "200 OK"
                );
            }
            else {

                sendResponse(
                    clientSocket,
                    makeError(
                        "Could not load archives."
                    ),
                    "500 Internal Server Error"
                );
            }
        }


        // POST scan
        else if (method == "POST" &&
                 path == "/api/scan") {

            std::string folderPath =
                getJsonString(
                    body,
                    "folderPath"
                );

            int inactiveDays =
                getJsonInt(
                    body,
                    "inactiveDays"
                );

            if (folderPath.empty() ||
                inactiveDays <= 0) {

                sendResponse(
                    clientSocket,
                    makeError(
                        "Invalid scan parameters."
                    ),
                    "400 Bad Request"
                );
            }
            else {

                ApplicationResult result;

                bool success =
                    runScanAndArchive(
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


        // POST restore
        else if (method == "POST" &&
                 path == "/api/restore") {

            std::string archiveId =
                getJsonString(
                    body,
                    "archiveId"
                );

            std::string destination =
                getJsonString(
                    body,
                    "destination"
                );

            if (archiveId.empty() ||
                destination.empty()) {

                sendResponse(
                    clientSocket,
                    makeError(
                        "Invalid restore parameters."
                    ),
                    "400 Bad Request"
                );
            }
            else {

                std::string restoredPath;

                bool success =
                    restoreFromArchive(
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
                        makeError(
                            "Restore operation failed."
                        ),
                        "400 Bad Request"
                    );
                }
            }
        }


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