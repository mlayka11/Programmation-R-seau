#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include "users.h"
#include "awale.h"
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include "games.h"
#include "commandes.h"
#define PORT 8000

static void handle_disconnect(int sock) {
    // Couper la partie en cours (ta nouvelle game_abort_by_socket remet déjà l’adversaire dispo)
    game_abort_by_socket(sock);

    // Marquer l’utilisateur OFFLINE + vider son opponent
    const char* me = get_username_by_socket(sock);
    if (me && me[0]) {
        set_user_socket(me, -1);
        user_set_state(me, U_OFFLINE);
        users_set_opponent(me, "");
    } else {
        // fallback si on n'a pas le pseudo
        set_socket_disconnected(sock);
    }
}

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
            handle_disconnect(new_socket); // marquer ce socket comme déconnecté
            break;
        }
        buffer[valread] = '\0';
        printf("Reçu : %s", buffer);
        char cmd[16] = {0};
        char username[1024] = {0};
        // Extraction de la commande
        sscanf(buffer, "%15s %1023s", cmd, username);
        if (strcmp(cmd, "LOGIN") == 0) {
            cmd_LOGIN(new_socket, current_user, sizeof(current_user), buffer);
        }
        else if (strcmp(cmd, "LIST") == 0) {
            cmd_LIST(new_socket);
        }
        else if (strcmp(cmd, "CHALLENGE") == 0) {
            cmd_CHALLENGE(new_socket, current_user, buffer);
        }
        else if (strcmp(cmd, "ACCEPT") == 0) {
            cmd_ACCEPT(new_socket, current_user, buffer);
        }
        else if (strcmp(cmd, "REFUSE") == 0) {
            cmd_REFUSE(new_socket, current_user, buffer);
        }
        else if (strcmp(cmd, "QUIT") == 0) {
            send(new_socket, "Déconnexion réussie.\n", 22, 0);
            printf(">> %s s'est déconnecté proprement.\n", current_user[0] ? current_user : "Client inconnu");
            handle_disconnect(new_socket);
            close(new_socket);
        }
        else if (strcmp(cmd, "MOVE") == 0) {
            cmd_MOVE(new_socket, current_user, buffer);
        }
        else if (strcmp(cmd, "GAMES") == 0) {
            cmd_GAMES(new_socket);
        }
        else if (strcmp(cmd, "OBSERVE") == 0) {
            cmd_OBSERVE(new_socket, current_user, buffer);
        }
        else if (strcmp(cmd, "UNOBSERVE") == 0) {
            cmd_UNOBSERVE(new_socket);
        }
        else if (strcmp(cmd, "MSG") == 0) {
            cmd_MSG(new_socket, current_user, buffer);
        }
        else if (strcmp(cmd, "BROADCAST") == 0) {
            cmd_BROADCAST(new_socket, current_user, buffer);
        }
        else if (strcmp(cmd, "SETBIO") == 0) {
            cmd_SETBIO(new_socket, current_user, buffer);
        }
        else if (strcmp(cmd, "SHOWBIO") == 0) {
            cmd_SHOWBIO(new_socket, buffer);
        }
        else if (strcmp(cmd, "ADDFRIEND") == 0) {
            cmd_ADDFRIEND(new_socket, current_user, buffer);
        }
        else if (strcmp(cmd, "PRIVATEON") == 0) {
            cmd_PRIVATEON(new_socket, current_user);
        }
        else if (strcmp(cmd, "PRIVATEOFF") == 0) {
            cmd_PRIVATEOFF(new_socket, current_user);
        }
        else if (strcmp(cmd, "HISTORY") == 0) {
            cmd_HISTORY(new_socket);
        }
        else {
            send(new_socket, "ERREUR : commande inconnue\n", 29, 0);
        }}
    /* Retire ce socket de toutes les listes d’observateurs */
    pthread_mutex_lock(&games_mutex);
    for (int i=0;i<MAX_GAMES;i++) {
        if (!games[i].active) continue;
        game_remove_spectator(i, new_socket);
    }
    pthread_mutex_unlock(&games_mutex);
    close(new_socket);
    printf("Thread terminé proprement pour %s.\n",
           current_user[0] ? current_user : "client inconnu");

    pthread_exit(NULL);
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
    //if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    //   perror("setsockopt");
      //  exit(EXIT_FAILURE);
    //}

    /* Réutiliser le port/adresse */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEPORT");
        /* SO_REUSEPORT peut échouer selon la conf; ce n’est pas bloquant, ne fais pas forcément exit ici */
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
	srand((unsigned)time(NULL));
	games_init();
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
