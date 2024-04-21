#include <iostream>
#include <filesystem>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <set>

namespace fs = std::filesystem;

const char* SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 8081;
const int BUF_LEN = (1024 * (sizeof(struct inotify_event) + 16));

std::string getFileList(const std::string& directory, std::set<std::string>& sentFiles) {
    std::string fileListStr;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (fs::is_regular_file(entry)) {
            std::string filename = entry.path().filename().string();
            if (sentFiles.find(filename) == sentFiles.end()) {
                fileListStr += filename + " " + std::to_string(fs::file_size(entry)) + "\n";
                sentFiles.insert(filename);
            }
        }
    }
    return fileListStr;
}

void sendFileListToServer(int sockfd, const std::string& folderPath, std::set<std::string>& sentFiles) {
    std::string fileListStr = getFileList(folderPath, sentFiles);
    ssize_t bytesSent = send(sockfd, fileListStr.c_str(), fileListStr.length(), 0);
    if (bytesSent == -1) {
        perror("send");
    }
}

int main() {
    const std::string folderPath = "/home/bonice/Bureau/Master 1/systeme distribues/projet3/data";
    std::set<std::string> sentFiles;

    int sockfd = -1;
    struct sockaddr_in serv_addr;

    int fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        return 1;
    }

    int wd = inotify_add_watch(fd, folderPath.c_str(), IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1) {
        perror("inotify_add_watch");
        close(fd);
        return 1;
    }

    char buffer[BUF_LEN];
    while (true) {
        // Vérifier si la connexion doit être (re)établie
        if (sockfd < 0) {
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                perror("socket");
                return 1;
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(SERVER_PORT);
            if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
                perror("inet_pton");
                close(sockfd);
                sockfd = -1; // Marquer la socket comme fermée
                continue;
            }

            if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("connect");
                close(sockfd);
                sockfd = -1; // Marquer la socket comme fermée
                continue;
            }
        }

        int length = read(fd, buffer, BUF_LEN);
        if (length < 0) {
            perror("read");
            break;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            if (event->len) {
                if (event->mask & IN_CREATE || event->mask & IN_DELETE || event->mask & IN_MODIFY || event->mask & IN_MOVED_FROM || event->mask & IN_MOVED_TO) {
                    sendFileListToServer(sockfd, folderPath, sentFiles);
                }
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    if (sockfd >= 0) {
        close(sockfd);
    }

    return 0;
}

