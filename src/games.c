#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include "games.h"

Game games[MAX_GAMES];
pthread_mutex_t games_mutex = PTHREAD_MUTEX_INITIALIZER;
static int next_game_id = 1;

/* --- Initialisation --- */
void games_init(void) {
    pthread_mutex_lock(&games_mutex);
    for (int i = 0; i < MAX_GAMES; i++) {
        games[i].active = 0;
        games[i].id = 0;
        games[i].spec_count = 0;
    }
    pthread_mutex_unlock(&games_mutex);
}

/* --- Recherche --- */
int game_find_by_user(const char *user) {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].active &&
            (strcmp(games[i].a, user) == 0 || strcmp(games[i].b, user) == 0))
            return i;
    }
    return -1;
}

int game_find_by_socket(int sock) {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!games[i].active) continue;
        if (games[i].sock_a == sock || games[i].sock_b == sock)
            return i;
    }
    return -1;
}

int game_find_by_id(int id) {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].active && games[i].id == id)
            return i;
    }
    return -1;
}

/* --- Création / fin --- */
int game_new(const char *a, int sock_a, const char *b, int sock_b) {
    pthread_mutex_lock(&games_mutex);
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!games[i].active) {
            games[i].active = 1;
            games[i].id = next_game_id++;
            strncpy(games[i].a, a, sizeof(games[i].a)-1);
            strncpy(games[i].b, b, sizeof(games[i].b)-1);
            games[i].sock_a = sock_a;
            games[i].sock_b = sock_b;
            games[i].spec_count = 0;
            for (int k=0;k<MAX_SPECTATORS;k++) games[i].specs[k] = -1;
            aw_init(games[i].board, &games[i].p1, &games[i].p2, &games[i].player);
            pthread_mutex_unlock(&games_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&games_mutex);
    return -1;
}

void game_end(int idx) {
    pthread_mutex_lock(&games_mutex);
    games[idx].active = 0;
    games[idx].id = 0;
    games[idx].spec_count = 0;
    pthread_mutex_unlock(&games_mutex);
}

/* --- Abandon / suppression --- */
void game_abort_by_socket(int sock) {
    int gi = game_find_by_socket(sock);
    if (gi < 0) return;

    // Identifier les deux joueurs
    const char *a = games[gi].a;
    const char *b = games[gi].b;
    int sa = games[gi].sock_a;
    int sb = games[gi].sock_b;

    // Déterminer qui s’est déconnecté
    const char *disconnected = NULL;
    const char *opponent = NULL;
    int opp_sock = -1;

    if (sock == sa) {
        disconnected = a;
        opponent = b;
        opp_sock = sb;
    } else {
        disconnected = b;
        opponent = a;
        opp_sock = sa;
    }

    // Notifier les deux (par sécurité)
    if (sa >= 0) send(sa, "GAME_ABORT\n", 11, 0);
    if (sb >= 0) send(sb, "GAME_ABORT\n", 11, 0);

    // Informer l’adversaire que son opposant a quitté
    if (opp_sock >= 0 && opponent) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "INFO : %s s'est déconnecté(e), la partie est terminée.\n",
                 disconnected ? disconnected : "adversaire");
        send(opp_sock, msg, strlen(msg), 0);
    }

    // Remettre l’adversaire disponible
    if (opponent && opponent[0]) {
        user_set_state(opponent, U_AVAILABLE);
        users_set_opponent(opponent, "");
    }

    // Marquer le joueur déconnecté comme hors ligne
    if (disconnected && disconnected[0]) {
        set_user_socket(disconnected, -1);
        user_set_state(disconnected, U_OFFLINE);
        users_set_opponent(disconnected, "");
    }

    // Supprimer la partie
    game_end(gi);
    printf(">> Partie interrompue : %s vs %s\n", a, b);
}

/* --- Observateurs --- */
void game_add_spectator(int idx, int sock) {
    pthread_mutex_lock(&games_mutex);
    if (games[idx].spec_count < MAX_SPECTATORS) {
        for (int i=0;i<games[idx].spec_count;i++)
            if (games[idx].specs[i] == sock) { pthread_mutex_unlock(&games_mutex); return; }
        games[idx].specs[games[idx].spec_count++] = sock;
    }
    pthread_mutex_unlock(&games_mutex);
}

void game_remove_spectator(int idx, int sock) {
    for (int i = 0; i < games[idx].spec_count; i++) {
        if (games[idx].specs[i] == sock) {
            games[idx].specs[i] = games[idx].specs[games[idx].spec_count - 1];
            games[idx].spec_count--;
            break;
        }
    }
}

/* --- État de jeu --- */
void game_send_state(int idx) {
    char buf[512];
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
    for (int i=0;i<games[idx].spec_count;i++) {
        int s = games[idx].specs[i];
        if (s >= 0) send(s, buf, strlen(buf), 0);
    }
}

/* --- Historique --- */
void save_game_to_csv(const char *playerA, const char *playerB,
                      int scoreA, int scoreB, const char *winner)
{
    FILE *f = fopen("data/games_history.csv", "a");
    if (!f) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char date[32];
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(f, "%s,%s,%d,%s,%d,%s,%s\n",
            date, playerA, scoreA, playerB, scoreB, winner,
            (scoreA == scoreB) ? "DRAW" : "WIN");
    fclose(f);
}

void remove_unallowed_spectators(const char *username) {
    pthread_mutex_lock(&games_mutex);
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!games[i].active) continue;

        // Si le joueur est impliqué dans cette partie
        if (strcmp(games[i].a, username) == 0 || strcmp(games[i].b, username) == 0) {
            for (int s = 0; s < games[i].spec_count; ) {
                int sock = games[i].specs[s];
                const char *spectator = get_username_by_socket(sock);

                // Si pas d'identité, ou pas ami autorisé => on le vire
                if (!spectator || !is_friend_allowed(username, spectator)) {
                    send(sock, "INFO : cette partie est devenue privée, vous ne pouvez plus observer.\n", 75, 0);
                    game_remove_spectator(i, sock);
                    continue; // ne pas incrémenter s car la liste se compresse
                }
                s++;
            }
        }
    }
    pthread_mutex_unlock(&games_mutex);
}