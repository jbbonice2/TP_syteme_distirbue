#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include <cstdio>
#include <cstring>
#include <cstdlib> 
#include <syslog.h> 

std::string filename = "data.txt"; // Nom du fichier par défaut
std::mutex dataMutex; // Mutex pour la synchronisation des données

std::string extractIP(const std::string& line);
void supprimerPartie(const std::string& fichier, const std::string& ip_a_supprimer);

void supprimerPartie(const std::string& fichier, const std::string& ip_a_supprimer) {
    // Fonction pour supprimer une partie du fichier
    std::ifstream ifs(fichier);
    std::ofstream ofs("temp.txt"); // fichier temporaire pour stocker les lignes sans la partie à supprimer
    std::string ligne;
    bool supprimer = false; // indique si nous devons supprimer la partie ou non

    while (std::getline(ifs, ligne)) {
        if (supprimer) {
            if (ligne.find("IP: ") == 0) {
                supprimer = false; // nous avons rencontré une autre ligne commençant par "IP:", donc nous arrêtons la suppression
            }
        } else {
            if (ligne.find("IP: " + ip_a_supprimer) != std::string::npos) {
                supprimer = true; // nous avons trouvé l'adresse IP spécifiée, nous devons commencer la suppression
                continue; // passer à la prochaine itération
            }
            ofs << ligne << std::endl; // écrire la ligne dans le fichier temporaire si nous ne devons pas la supprimer
        }
    }

    ifs.close();
    ofs.close();

    // Renommer le fichier temporaire pour remplacer le fichier d'origine
    std::rename("temp.txt", fichier.c_str());
}

void saveData(const std::string& fileName, const std::string& data, const std::string& clientInfo) {
    // Fonction pour sauvegarder les données
    std::lock_guard<std::mutex> lock(dataMutex);

    // Ouvrir le fichier en mode append
    std::ofstream outFile(fileName, std::ios::app);
    if (!outFile.is_open()) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier de sortie " << fileName << std::endl;
        return;
    }

    // Écrire les informations du client dans le fichier
    outFile << "IP: " << clientInfo << "\n" << data << std::endl;

    // Fermer le fichier
    outFile.close();
    unlock(dataMutex);
}

std::string extractIP(const std::string& line) {
    // Fonction pour extraire l'adresse IP
    size_t pos = line.find("IP: ");
    if (pos != std::string::npos) {
        size_t startPos = pos + 4;
        size_t endPos = line.find(':', startPos);
        if (endPos != std::string::npos) {
            return line.substr(startPos, endPos - startPos);
        }
    }
    return "";
}

void handleClient(int clientSocket) {
    // Fonction pour gérer le client
    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    getpeername(clientSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);

    char ipAddress[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), ipAddress, INET_ADDRSTRLEN);
    std::string ipAddressStr(ipAddress);
    std::string port = std::to_string(ntohs(clientAddr.sin_port));
    std::string clientInfo = ipAddressStr + ":" + port;
    std::string filename = "data.txt";

    char buffer[1024];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRead == -1) {
        syslog(LOG_ERR, "Error receiving data");
        close(clientSocket);
        return;
    }
    std::cout << "Data received from client:\n" << clientInfo << "\n" << std::string(buffer, bytesRead)  << std::endl; 	
    syslog(LOG_INFO, "Data received from client:\n%s\n%s", clientInfo.c_str(), std::string(buffer, bytesRead).c_str());
    supprimerPartie(filename, ipAddressStr);  
    saveData(filename, std::string(buffer, bytesRead), clientInfo);
    
    close(clientSocket);
}

int main(int argc, char *argv[]) {
    openlog("serveur", LOG_PID | LOG_NDELAY, LOG_LOCAL0);

    int port = 8080; // Port par défaut

    // Analyser les options de ligne de commande
    int opt;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg); // Convertir optarg en entier
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-p port_number]" << std::endl;
                return 1;
        }
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to create file " << filename << std::endl;
        return -1;
    }
    file.close();

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        syslog(LOG_ERR, "Error: Failed to create socket");
        return -1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port); // Utiliser le numéro de port analysé

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        syslog(LOG_ERR, "Error: Failed to bind socket");
        close(serverSocket);
        return -1;
    }

    if (listen(serverSocket, 5) == -1) {
        syslog(LOG_ERR, "Error: Failed to listen");
        close(serverSocket);
        return -1;
    }
    std::cout << "Server listening on port " << port << std::endl;
    syslog(LOG_INFO, "Server listening on port %d", port);

    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == -1) {
            syslog(LOG_ERR, "Error: Failed to accept connection");
            continue;
        }

        syslog(LOG_INFO, "New connection accepted");

        std::thread clientThread(handleClient, clientSocket);
        clientThread.join();
    }

    closelog();

    return 0;
}
