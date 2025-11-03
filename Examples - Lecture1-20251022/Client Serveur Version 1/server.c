#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include "users.h"

#define PORT 8080

// --- Fonction thread : gère un client ---
static void* client_handler(void* arg) { // arg transporte le socket vers le thread
    int new_socket = *(int*)arg;
    free(arg);

    char buffer[1024];
    int valread;
    char current_user[64] = {0}; // pseudo associé à CE socket après LOGIN

    while (1) {

        memset(buffer, 0, sizeof(buffer));

        valread = read(new_socket, buffer, sizeof(buffer) - 1);
        if (valread <= 0) {
            printf("Client déconnecté.\n");
            set_socket_disconnected(new_socket); // marquer ce socket comme déconnecté
            break;
        }

        buffer[valread] = '\0';
        printf("Reçu : %s", buffer);

        char cmd[16] = {0};
        char username[1024] = {0};

        // Extraction de la commande
        sscanf(buffer, "%15s %1023s", cmd, username);

        if (strcmp(cmd, "LOGIN") == 0) {
            if (sscanf(buffer, "%*s %1023s", username) == 1) {
                if (!is_valid_username(username)) {
                    send(new_socket, "ERREUR : pseudo invalide\n", 26, 0);
                } else if (username_exists(username)) {
                    send(new_socket, "ERREUR : pseudo déjà utilisé\n", 31, 0);
                } else {
                    if (!add_user(username)) {
                        send(new_socket, "ERREUR : serveur plein\n", 24, 0);
                    } else {
                        // Associer le pseudo à ce socket et mémoriser le user courant
                        set_user_socket(username, new_socket);
                        strncpy(current_user, username, sizeof(current_user) - 1);

                        send(new_socket, "Connexion réussie !\n", 22, 0);
                        printf("Nouvel utilisateur : %s (total = %d)\n",
                               username, get_user_count());
                    }
                }
            } else {
                send(new_socket, "ERREUR : pseudo manquant\n", 26, 0);
            }
        }

        else if (strcmp(cmd, "LIST") == 0) {
            char listbuf[1024];
            get_user_list(listbuf, sizeof(listbuf));
            send(new_socket, listbuf, strlen(listbuf), 0);
        }

        // --- Défi : A défie B ---
        else if (strcmp(cmd, "CHALLENGE") == 0) {
            char target[64] = {0};
            if (sscanf(buffer, "%*s %63s", target) != 1) {
                send(new_socket, "ERREUR : pseudo cible manquant\n", 31, 0);
            } else if (current_user[0] == '\0') {
                send(new_socket, "ERREUR : connecte-toi d'abord (LOGIN)\n", 38, 0);
            } else if (strcmp(current_user, target) == 0) {
                send(new_socket, "ERREUR : tu ne peux pas te défier toi-même\n", 45, 0);
            } else if (!username_exists(target)) {
                send(new_socket, "ERREUR : joueur introuvable\n", 29, 0);
            } else {
                int tsock = get_user_socket(target);
                if (tsock < 0) {
                    send(new_socket, "ERREUR : joueur non disponible\n", 31, 0);
                } else {
                    char notif[128];
                    snprintf(notif, sizeof(notif), "CHALLENGE_FROM %s\n", current_user);
                    send(tsock, notif, strlen(notif), 0);
                    send(new_socket, "CHALLENGE_SENT\n", 15, 0);
                }
            }
        }

        // --- Acceptation de défi ---
        else if (strcmp(cmd, "ACCEPT") == 0) {
            char opponent[64] = {0};
            if (sscanf(buffer, "%*s %63s", opponent) != 1) {
                send(new_socket, "ERREUR : pseudo adversaire manquant\n", 36, 0);
            } else if (current_user[0] == '\0') {
                send(new_socket, "ERREUR : connecte-toi d'abord (LOGIN)\n", 38, 0);
            } else {
                int osock = get_user_socket(opponent);
                if (osock < 0) {
                    send(new_socket, "ERREUR : adversaire indisponible\n", 33, 0);
                } else {
                    char ok[160];
                    snprintf(ok, sizeof(ok), "CHALLENGE_ACCEPTED %s %s\n", opponent, current_user);
                    send(new_socket, ok, strlen(ok), 0);
                    send(osock,   ok, strlen(ok), 0);
                    // TODO : créer la partie, choisir qui commence, etc.
                }
            }
        }

        // --- Refus de défi ---
        else if (strcmp(cmd, "REFUSE") == 0) {
            char opponent[64] = {0};
            if (sscanf(buffer, "%*s %63s", opponent) != 1) {
                send(new_socket, "ERREUR : pseudo adversaire manquant\n", 36, 0);
            } else if (current_user[0] == '\0') {
                send(new_socket, "ERREUR : connecte-toi d'abord (LOGIN)\n", 38, 0);
            } else {
                int osock = get_user_socket(opponent);
                if (osock >= 0) {
                    char r[160];
                    snprintf(r, sizeof(r), "CHALLENGE_REFUSED %s %s\n", opponent, current_user);
                    send(osock, r, strlen(r), 0);
                }
                send(new_socket, "REFUSE_OK\n", 10, 0);
            }
        }

        else if (strcmp(cmd, "QUIT") == 0) {
            send(new_socket, "Déconnexion réussie.\n", 22, 0);
            printf("Client a quitté la session.\n");
            set_socket_disconnected(new_socket);
            break;
        }

        else {
            send(new_socket, "ERREUR : commande inconnue\n", 29, 0);
        }
    }

    close(new_socket);
    printf("Connexion terminée.\n");
    return NULL;
}

int main(int argc, char const* argv[])
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    init_users(10);

    // Création du socket serveur
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Réutiliser le port en cas de redémarrage
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Attacher la socket au port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Mise en écoute
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Serveur démarré sur le port %d...\n", PORT);

    // Boucle principale : accepter plusieurs clients
    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        printf("Nouvelle connexion acceptée.\n");

        // Création d’un thread pour le client
        pthread_t tid;
        int* socket_ptr = (int*)malloc(sizeof(int));
        if (!socket_ptr) {
            perror("malloc");
            close(new_socket);
            continue;
        }
        *socket_ptr = new_socket;

        if (pthread_create(&tid, NULL, client_handler, socket_ptr) != 0) {
            perror("pthread_create");
            close(new_socket);
            free(socket_ptr);
            continue;
        }

        // On détache le thread pour ne pas avoir à le rejoindre
        pthread_detach(tid);
    }

    shutdown(server_fd, SHUT_RDWR); // fermeture du socket serveur
    return 0;
}
