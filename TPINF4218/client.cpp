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
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdlib> // For atoi
#include <cstring> // For strlen

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

// Fonction pour recevoir et afficher les données du serveur
void receiveAndDisplayData(int client_socket) {
    char buffer[1024] = {0};
    read(client_socket, buffer, sizeof(buffer));
    std::cout << "Data received from server:\n" << buffer << std::endl;
}

// Function to send data to the server
void sendDataToServer(int clientSocket, const std::string& data) {
    send(clientSocket, data.c_str(), data.size(), 0);
}

int main(int argc, char *argv[]) {
    int opt;
    std::string serverIP;
    int serverPort = 0; // Initialize to zero to check later
    std::string filename; // Declare filename outside switch block

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
        std::cerr << "Server IP address or port missing or invalid.\n";
        return -1;
    }

    openlog("client", LOG_PID, LOG_USER);

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0); // Create socket
    if (clientSocket == -1) {
        std::cerr << "Error: Failed to create socket\n";
        syslog(LOG_ERR, "Error: Failed to create socket");
        closelog();
        return -1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Error: Invalid address or address not supported\n";
        syslog(LOG_ERR, "Error: Invalid address or address not supported");
        close(clientSocket);
        closelog();
        return -1;
    }

    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error: Connection failed\n";
        syslog(LOG_ERR, "Error: Connection failed");
        close(clientSocket);
        closelog();
        return -1;
    }

    std::string directory = "data";
    const char *message = "Send data";

    while (true) {
        // Generate the new list of files
        std::string currentFileListStr = getFileList(directory);

        // Display menu options
        std::cout << "\nMenu:\n";
        std::cout << "1. present list file\n";
        std::cout << "2. see file list\n";
        std::cout << "3. Download file from server\n";
        std::cout << "4. Exit\n";
        std::cout << "Enter your choice: ";

        int choice;
        std::cin >> choice;

        switch (choice) {
            case 1:
                // Send the new file list to the server
                sendDataToServer(clientSocket, currentFileListStr);
                break;
            case 2:
                // Send request to server
                sendDataToServer(clientSocket, message);
                // Receive and display data from server
                receiveAndDisplayData(clientSocket);
                break;
            case 3:
              
                break;
            case 4:
                // Exit
                std::cout << "Exiting program.\n";
                close(clientSocket);
                closelog();
                return 0;
            default:
                std::cerr << "Invalid choice. Please enter a number between 1 and 4.\n";
        }
    }

    close(clientSocket);
    closelog();
    return 0;
}

