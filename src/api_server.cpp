#include "api_server.h"

#include "vault_report.h"

#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
    std::string readRequest(int clientSocket)
    {
        char buffer[4096];

        int bytesRead =
            recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead <= 0) {
            return "";
        }

        buffer[bytesRead] = '\0';

        return std::string(buffer);
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

    std::string readJsonFile(const std::string& path)
    {
        std::ifstream file(path);

        if (!file.is_open()) {
            return "";
        }

        std::string data;
        std::string line;

        while (std::getline(file, line)) {
            data += line;

            if (!file.eof()) {
                data += "\n";
            }
        }

        file.close();

        return data;
    }

    void sendResponse(int clientSocket,
                      const std::string& body,
                      const std::string& status)
    {
        std::string response;

        response += "HTTP/1.1 " + status + "\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "Content-Length: "
                  + std::to_string(body.size())
                  + "\r\n";
        response += "Connection: close\r\n";
        response += "\r\n";
        response += body;

        send(
            clientSocket,
            response.c_str(),
            response.size(),
            0
        );
    }

    std::string makeErrorJson(const std::string& message)
    {
        return
            "{\n"
            "  \"success\": false,\n"
            "  \"message\": \"" +
            message +
            "\"\n"
            "}";
    }
}

bool startApiServer(const std::string& archiveRoot,
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
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    if (bind(
            serverSocket,
            (sockaddr*)&serverAddress,
            sizeof(serverAddress)) < 0) {

        std::cout << "Could not bind server to port "
                  << port << ".\n";

        close(serverSocket);
        return false;
    }

    if (listen(serverSocket, 5) < 0) {
        std::cout << "Could not start listening.\n";

        close(serverSocket);
        return false;
    }

    std::cout << "SmartFileVault API server started.\n";
    std::cout << "Open: http://localhost:"
              << port << "\n\n";

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

        std::string path =
            getPath(request);

        if (path == "/api/dashboard") {

            std::string reportPath =
                archiveRoot + "/vault_report.json";

            if (saveVaultReport(
                    archiveRoot,
                    reportPath)) {

                std::string json =
                    readJsonFile(reportPath);

                sendResponse(
                    clientSocket,
                    json,
                    "200 OK"
                );
            }
            else {

                sendResponse(
                    clientSocket,
                    makeErrorJson(
                        "Could not create vault report."
                    ),
                    "500 Internal Server Error"
                );
            }
        }

        else if (path == "/api/archives") {

            std::string reportPath =
                archiveRoot + "/vault_report.json";

            if (saveVaultReport(
                    archiveRoot,
                    reportPath)) {

                std::string json =
                    readJsonFile(reportPath);

                sendResponse(
                    clientSocket,
                    json,
                    "200 OK"
                );
            }
            else {

                sendResponse(
                    clientSocket,
                    makeErrorJson(
                        "Could not load archives."
                    ),
                    "500 Internal Server Error"
                );
            }
        }

        else {

            sendResponse(
                clientSocket,
                makeErrorJson("Endpoint not found."),
                "404 Not Found"
            );
        }

        close(clientSocket);
    }

    close(serverSocket);

    return true;
}