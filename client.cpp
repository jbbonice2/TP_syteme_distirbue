#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <filesystem>
#include <syslog.h>

namespace fs = std::filesystem;

// Structure to store filename and size
struct FileData {
    std::string filename;
    long long size; // Using long long to store large file sizes
};

// Function to generate the list of files and their sizes
std::string getFileList(const std::string& directory) {
    std::string fileListStr;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (fs::is_regular_file(entry)) {
            FileData fileData;
            fileData.filename = entry.path().filename().string();
            fileData.size = fs::file_size(entry);
            fileListStr += fileData.filename + " " + std::to_string(fileData.size) + "\n";
        }
    }
    return fileListStr;
}

int main(int argc, char *argv[]) {
    int opt;
    std::string serverIP;
    int serverPort;

    // Parsing command line options
    while ((opt = getopt(argc, argv, "i:p:")) != -1) {
        switch (opt) {
            case 'i':
                serverIP = optarg;
                break;
            case 'p':
                serverPort = atoi(optarg);
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " -i <server IP> -p <port>\n";
                return -1;
        }
    }

    if (serverIP.empty() || serverPort == 0) {
        std::cerr << "Server IP address or port missing.\n";
        return -1;
    }

    openlog("client", LOG_PID, LOG_USER);

    std::string directory = "data";
    std::string prevFileListStr;
    bool firstRun = true;

    while (true) {
        // Generate the new list of files
        std::string currentFileListStr = getFileList(directory);

        // Check for any modifications
        if (firstRun || currentFileListStr != prevFileListStr) {
            syslog(LOG_INFO, "New file list generated.");

            int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (clientSocket == -1) {
                syslog(LOG_ERR, "Error: Failed to create socket");
                return -1;
            }

            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(serverPort);
            inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

            if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
                syslog(LOG_ERR, "Error: Connection failed");
                close(clientSocket);
                return -1;
            }

            // Send the new file list to the server
            send(clientSocket, currentFileListStr.c_str(), currentFileListStr.size(), 0);
            close(clientSocket);

            // Update the previous list with the new list
            prevFileListStr = currentFileListStr;

            // Mark that it's no longer the first run
            firstRun = false;
        }

        // Wait for a few seconds before checking again
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    closelog();
    return 0;
}

