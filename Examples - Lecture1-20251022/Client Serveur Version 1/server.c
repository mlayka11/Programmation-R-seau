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


#define PORT 8080
#define MAX_GAMES 128

typedef struct {
    int active;
    char a[64], b[64];   // pseudos
    int sock_a, sock_b;  // sockets
    int board[12];
    int p1, p2;          // scores
    int player;          // 1 (J1=a) ou 2 (J2=b)
} Game;

static Game games[MAX_GAMES];
static pthread_mutex_t games_mutex = PTHREAD_MUTEX_INITIALIZER;


static void games_init(void) {
    for (int i = 0; i < MAX_GAMES; i++) games[i].active = 0;
}

static int game_find_by_user(const char *user) {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].active &&
            (strcmp(games[i].a, user) == 0 || strcmp(games[i].b, user) == 0))
            return i;
    }
    return -1;
}




static int game_new(const char *a, int sock_a, const char *b, int sock_b) {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!games[i].active) {
            games[i].active = 1;
            strncpy(games[i].a, a, sizeof(games[i].a)-1);
            strncpy(games[i].b, b, sizeof(games[i].b)-1);
            games[i].a[sizeof(games[i].a)-1] = '\0';
            games[i].b[sizeof(games[i].b)-1] = '\0';
            games[i].sock_a = sock_a;
            games[i].sock_b = sock_b;
            aw_init(games[i].board, &games[i].p1, &games[i].p2, &games[i].player);
            return i;
        }
    }
    return -1;
}

static void game_end(int idx) {
    games[idx].active = 0;
}
int game_find_by_socket(int sock) {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!games[i].active) continue;
        if (games[i].sock_a == sock || games[i].sock_b == sock)
            return i;
    }
    return -1;
}
void game_abort_by_socket(int sock) {
    int gi = game_find_by_socket(sock);
    if (gi < 0) return;

    // Notifie les deux (si encore ouverts)
    if (games[gi].sock_a >= 0) send(games[gi].sock_a, "GAME_ABORT\n", 11, 0);
    if (games[gi].sock_b >= 0) send(games[gi].sock_b, "GAME_ABORT\n", 11, 0);

    game_end(gi); // ta fonction qui libère/retire la partie et met active=0
}


static void game_send_state(int idx) {
    char buf[512];
    // envoie le plateau et les scores aux deux joueurs
    int *B = games[idx].board;
    snprintf(buf, sizeof(buf),
        "BOARD %d %d %d %d %d %d\n"
        "      %d %d %d %d %d %d | SCORE %d %d | TURN %s\n",
        B[11], B[10], B[9], B[8], B[7], B[6],
        B[0],  B[1],  B[2], B[3], B[4], B[5],
        games[idx].p1, games[idx].p2,
        (games[idx].player==1)?games[idx].a:games[idx].b
    );
    send(games[idx].sock_a, buf, strlen(buf), 0);
    send(games[idx].sock_b, buf, strlen(buf), 0);
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
            set_socket_disconnected(new_socket); // marquer ce socket comme déconnecté
            break;
        }
        buffer[valread] = '\0';
        printf("Reçu : %s", buffer);
        char cmd[16] = {0};
        char username[1024] = {0};
        // Extraction de la commande
        sscanf(buffer, "%15s %1023s", cmd, username);
        /*     LOGIN        */
        if (strcmp(cmd, "LOGIN") == 0) {
            trim_inplace(username);
            if (sscanf(buffer, "%*s %1023s", username) == 1) {
                
                if (!is_valid_username(username)) {
                    send(new_socket, "ERREUR : pseudo invalide\n", 26, 0);
                } else if (username_exists(username)) {
                    send(new_socket, "ERREUR : pseudo déjà utilisé\n", 31, 0);
                } else {
                    if (!add_user(username)) {
                        send(new_socket, "ERREUR : serveur plein\n", 24, 0);
                    } else {
                        set_user_socket(username, new_socket);
                        user_set_state(username, U_AVAILABLE);     
                        users_set_opponent(username, "");   
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
        /*     LIST        */
        else if (strcmp(cmd, "LIST") == 0) {
            char listbuf[1024];
            get_user_list(listbuf, sizeof(listbuf));
            send(new_socket, listbuf, strlen(listbuf), 0);
        }
        /*     CHALLENGE       */
        else if (strcmp(cmd, "CHALLENGE") == 0) {
            char target[64] = {0};
            if (sscanf(buffer, "%*s %63s", target) != 1) {
                send(new_socket, "ERREUR : pseudo cible manquant\n", 31, 0);
            } else {
                trim_inplace(target);
                trim_inplace(current_user);

                if (current_user[0] == '\0') {
                    send(new_socket, "ERREUR : connecte-toi d'abord (LOGIN)\n", 38, 0);
                } else if (strcmp(current_user, target) == 0) {
                    send(new_socket, "ERREUR : tu ne peux pas te défier toi-même\n", 45, 0);
                } else if (!username_exists(target)) {
                    send(new_socket, "ERREUR : joueur introuvable\n", 29, 0);
                } else {
                    int tsock = get_user_socket(target);
                    if (tsock < 0 || fcntl(tsock, F_GETFD) == -1) {
                        send(new_socket, "ERREUR : joueur non disponible\n", 31, 0);
                    }
                    // Vérifie que chacun est disponible (pas pending / in_game)
                    else if (!user_is_available(current_user)) {
                        send(new_socket, "ERREUR : tu es occupé\n", 22, 0);
                    } else if (!user_is_available(target)) {
                        send(new_socket, "ERREUR : joueur occupé\n", 23, 0);
                    } else {
                        // Marquer les deux en attente (PENDING) de façon atomique
                        if (user_set_pending_pair(current_user, target) < 0) {
                            send(new_socket, "ERREUR : état changé, réessaie\n", 31, 0);
                        } else {
                            char notif[128];
                            snprintf(notif, sizeof(notif), "CHALLENGE_FROM %s\n", current_user);
                            send(tsock, notif, strlen(notif), 0);
                            send(new_socket, "CHALLENGE_SENT\n", 15, 0);
                        }
                    }
                }
            }
}


        /*     ACCEPT       */
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
            // Notifier acceptation (comme tu le faisais)
            char ok[160];
            snprintf(ok, sizeof(ok), "CHALLENGE_ACCEPTED %s %s\n", opponent, current_user);
            send(new_socket, ok, strlen(ok), 0);
            send(osock,   ok, strlen(ok), 0);

            // === CRÉER LA PARTIE ===
            int idx = game_new(opponent, osock, current_user, new_socket);
            if (idx < 0) {
                send(new_socket, "ERREUR : trop de parties\n", 26, 0);
            } else {
                // Tirage aléatoire : 50% on inverse J1/J2 pour choisir qui commence
                user_set_in_game_pair(opponent, current_user); // met U_IN_GAME pour les deux + opponent=...
                if (rand() % 2) {
                    Game g = games[idx];
                    int sa = g.sock_a; g.sock_a = g.sock_b; g.sock_b = sa;
                    char tmpn[64]; strncpy(tmpn, g.a, 63); tmpn[63]='\0';
                    strncpy(g.a, g.b, 63); g.a[63]='\0';
                    strncpy(g.b, tmpn, 63); g.b[63]='\0';
                    games[idx] = g;
                    aw_init(games[idx].board, &games[idx].p1, &games[idx].p2, &games[idx].player);
                }

                char start[160];
                snprintf(start, sizeof(start), "GAME_START %s %s | STARTS %s\n",
                         games[idx].a, games[idx].b,
                         (games[idx].player==1)?games[idx].a:games[idx].b);
                send(games[idx].sock_a, start, strlen(start), 0);
                send(games[idx].sock_b, start, strlen(start), 0);
                game_send_state(idx);
            }
        }
    }
}


          /*     REFUSE       */
    else if (strcmp(cmd, "REFUSE") == 0) {
        char opponent[64] = {0};
        if (sscanf(buffer, "%*s %63s", opponent) != 1) {
            send(new_socket, "ERREUR : pseudo adversaire manquant\n", 36, 0);
        } else if (current_user[0] == '\0') {
            send(new_socket, "ERREUR : connecte-toi d'abord (LOGIN)\n", 38, 0);
        } else {
            trim_inplace(opponent);
            // On annule un défi en attente : remettre les deux à AVAILABLE
            user_clear_pair(current_user, opponent); // remet U_AVAILABLE + opponent=""

            int osock = get_user_socket(opponent);
            if (osock >= 0) {
                char r[160];
                snprintf(r, sizeof(r), "CHALLENGE_REFUSED %s %s\n", opponent, current_user);
                send(osock, r, strlen(r), 0);
            }
            send(new_socket, "REFUSE_OK\n", 10, 0);
        }
    }

        /*     QUIT      */
        else if (strcmp(cmd, "QUIT") == 0) {
            send(new_socket, "Déconnexion réussie.\n", 22, 0);
            printf("Client a quitté la session.\n");
            set_socket_disconnected(new_socket);
            break;
        }


        /*     MOVE      */
	    else if (strcmp(cmd, "MOVE") == 0) {
            int k = 0;
            if (sscanf(buffer, "%*s %d", &k) != 1) {
                send(new_socket, "ERREUR : MOVE <1..6>\n", 22, 0);
                continue;
            }
            if (current_user[0] == '\0') {
                send(new_socket, "ERREUR : connecte-toi d'abord (LOGIN)\n", 38, 0);
                continue;
            }

            int gi = game_find_by_user(current_user);
            if (gi < 0 || !games[gi].active) {
                send(new_socket, "ERREUR : aucune partie en cours\n", 32, 0);
                continue;
            }

            int isA = (strcmp(games[gi].a, current_user) == 0);
            int my_player = isA ? 1 : 2;

            if (games[gi].player != my_player) {
                send(new_socket, "ERREUR : pas ton tour\n", 23, 0);
                continue;
            }

            /* --- Mapping identique à ton jeu ncurses --- */
            int pit_index = -1;
            if (k >= 1 && k <= 6) {
                if (my_player == 1) {
                    pit_index = k - 1;      /* J1 : 1..6 -> 0..5 */
                } else {
                    pit_index = 5 + k;      /* J2 : 1..6 -> 6..11 (pit += 6 en 1-based) */
                }
            }
            if (pit_index < 0) {
                send(new_socket, "ERREUR : MOVE invalide (1..6)\n", 30, 0);
                continue;
            }
            /* Vérification et application (mêmes règles que ton game()) */
            if (!aw_is_legal(games[gi].board, games[gi].player, pit_index)) {
                send(new_socket, "ERREUR : coup illégal\n", 22, 0);
                continue;
            }
            int ended = aw_play(games[gi].board, &games[gi].p1, &games[gi].p2,
                                &games[gi].player, pit_index);

            game_send_state(gi);

            if (ended) {
                char over[160];
                const char *winner = (games[gi].p1 > games[gi].p2) ? games[gi].a :
                                    (games[gi].p2 > games[gi].p1) ? games[gi].b : "DRAW";
                snprintf(over, sizeof(over), "GAME_OVER WINNER %s | SCORE %d %d\n",
                        winner, games[gi].p1, games[gi].p2);
                send(games[gi].sock_a, over, strlen(over), 0);
                send(games[gi].sock_b, over, strlen(over), 0);
                user_clear_pair(games[gi].a, games[gi].b);
                game_end(gi);
            }
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
    //if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    //   perror("setsockopt");
      //  exit(EXIT_FAILURE);
    //}

    /* Réutiliser le port/adresse */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
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
