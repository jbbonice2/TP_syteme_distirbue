#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/inotify.h>
#include <libnotify/notify.h>

#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + 16))
int monitorFolder(const char *folderPath) {
    // Initialiser Notify
    notify_init("MonApplication");

    int fd, wd;
    char buffer[BUF_LEN];

    // Créer un point de surveillance inotify
    fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        return 1;
    }

    // Ajouter un point de surveillance pour le dossier spécifié
    wd = inotify_add_watch(fd, folderPath, IN_MODIFY | IN_CREATE | IN_DELETE);
    if (wd == -1) {
        perror("inotify_add_watch");
        close(fd); // Fermer le descripteur de fichier avant de retourner
        return 1;
    }

    // Boucle de surveillance des événements
    while (true) {
        int length = read(fd, buffer, BUF_LEN);
        if (length < 0) {
            perror("read");
            break;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event *event = reinterpret_cast<struct inotify_event *>(&buffer[i]);
            if (event->len) {
                if (event->mask & IN_MODIFY) {
                    // Fichier modifié
                    NotifyNotification *notification = notify_notification_new("Modification détectée", event->name, nullptr);
                    notify_notification_show(notification, nullptr);
                    g_object_unref(G_OBJECT(notification));
                }
                if (event->mask & IN_CREATE) {
                    // Fichier créé
                    NotifyNotification *notification = notify_notification_new("Création détectée", event->name, nullptr);
                    notify_notification_show(notification, nullptr);
                    g_object_unref(G_OBJECT(notification));
                }
                if (event->mask & IN_DELETE) {
                    // Fichier supprimé
                    NotifyNotification *notification = notify_notification_new("Suppression détectée", event->name, nullptr);
                    notify_notification_show(notification, nullptr);
                    g_object_unref(G_OBJECT(notification));
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }

    // Supprimer le point de surveillance et libérer les ressources
    inotify_rm_watch(fd, wd);
    close(fd);
    notify_uninit();

    return 0; // Aucune erreur, retourne 0
}
int main() {
    const char *folderPath = "/home/bonice/Bureau/Master 1/systeme distribues/projet2/data"; // Remplacez par votre propre chemin
    if (monitorFolder(folderPath) == 1) {
        std::cout << "Bonjour" << std::endl;
    }
    return 0;
}

