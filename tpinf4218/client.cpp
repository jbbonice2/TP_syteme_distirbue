#include <iostream>
#include <filesystem>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <syslog.h> // Ajout de l'en-tête pour la bibliothèque syslog

namespace fs = std::filesystem;

const int BUF_LEN = (1024 * (sizeof(struct inotify_event) + 16));

std::string getFileList(const std::string& directory) {
    std::string fileListStr;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (fs::is_regular_file(entry)) {
            fileListStr += entry.path().filename().string() + " " + std::to_string(fs::file_size(entry)) + "\n";
        }
    }
    return fileListStr;
}

void sendFileListToServer(int sockfd, const std::string& folderPath) {
    std::string fileListStr = getFileList(folderPath);
    ssize_t bytesSent = send(sockfd, fileListStr.c_str(), fileListStr.length(), 0);
    if (bytesSent == -1) {
        perror("send");
        syslog(LOG_ERR, "Erreur lors de l'envoi de la liste des fichiers au serveur.");
    } else {
        syslog(LOG_INFO, "Liste des fichiers envoyée au serveur avec succès.");
    }
}

int main(int argc , char *argv[]) {
    openlog("client", LOG_PID | LOG_CONS, LOG_USER); // Ouvrir une connexion vers le système de logging

    std::string folderPath = "/home/bonice/Bureau/Master 1/systeme distribues/projet3/data";
    const char* serverIP = "127.0.0.1";
    int serverPort = 8082;

    int opt;
    while ((opt = getopt(argc, argv, "i:p:d:")) != -1) {
        switch (opt) {
            case 'i':
                serverIP = optarg;
                break;
            case 'p':
                serverPort = std::stoi(optarg);
                break;
            case 'd':
                folderPath = optarg;
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-i server_ip] [-p server_port] [-d folder_path]" << std::endl;
                return 1;
        }
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        syslog(LOG_ERR, "Erreur lors de la création de la socket.");
        closelog();
        return 1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIP, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        syslog(LOG_ERR, "Erreur lors de la conversion de l'adresse IP.");
        close(sockfd);
        closelog();
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        syslog(LOG_ERR, "Erreur lors de la connexion au serveur.");
        close(sockfd);
        closelog();
        return 1;
    }
    
    sendFileListToServer(sockfd, folderPath);

    int fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        syslog(LOG_ERR, "Erreur lors de l'initialisation de inotify.");
        close(sockfd);
        closelog();
        return 1;
    }

    int wd = inotify_add_watch(fd, folderPath.c_str(), IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1) {
        perror("inotify_add_watch");
        syslog(LOG_ERR, "Erreur lors de l'ajout de la surveillance inotify.");
        close(fd);
        close(sockfd);
        closelog();
        return 1;
    }

    char buffer[BUF_LEN];
    while (true) {
        // Vérifier la connexion à chaque itération
        if (sockfd < 0 || (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)) {
            perror("Reconnecting...");
            syslog(LOG_WARNING, "Tentative de reconnexion au serveur...");
            close(sockfd);
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                perror("socket");
                syslog(LOG_ERR, "Erreur lors de la création de la socket.");
                closelog();
                return 1;
            }
            if (inet_pton(AF_INET, serverIP, &serv_addr.sin_addr) <= 0) {
                perror("inet_pton");
                syslog(LOG_ERR, "Erreur lors de la conversion de l'adresse IP.");
                close(sockfd);
                closelog();
                return 1;
            }
            continue;
        }

        int length = read(fd, buffer, BUF_LEN);
        if (length < 0) {
            perror("read");
            syslog(LOG_ERR, "Erreur lors de la lecture des événements inotify.");
            break;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            if (event->len) {
                if (event->mask & IN_CREATE || event->mask & IN_DELETE || event->mask & IN_MODIFY || event->mask & IN_MOVED_FROM || event->mask & IN_MOVED_TO) {
                    sendFileListToServer(sockfd, folderPath);
                }
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    close(sockfd);

    closelog(); // Fermer la connexion vers le système de logging

    return 0;
}

