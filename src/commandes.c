#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "commandes.h"
#include "users.h"
#include "games.h"
#include "awale.h"

/* ===== Helpers ===== */
static void send_txt(int sock, const char* s) {
    send(sock, s, strlen(s), 0);
}
static void sendf(int sock, const char* fmt, ...) {
    char out[2048];
    va_list ap; va_start(ap, fmt);
    vsnprintf(out, sizeof(out), fmt, ap);
    va_end(ap);
    send(sock, out, strlen(out), 0);
}

/* ===== Commandes ===== */

void cmd_LOGIN(int sock, char *current_user, size_t user_cap, const char *buffer) {
    char username[1024] = {0};
    if (sscanf(buffer, "%*s %1023s", username) != 1) {
        send_txt(sock, "ERREUR : pseudo manquant\n");
        return;
    }
    trim_inplace(username);
    if (!is_valid_username(username)) {
        send_txt(sock, "ERREUR : pseudo invalide\n");
        return;
    }
    if (username_exists(username)) {
        send_txt(sock, "ERREUR : pseudo déjà utilisé\n");
        return;
    }
    if (!add_user(username)) {
        send_txt(sock, "ERREUR : serveur plein\n");
        return;
    }
    set_user_socket(username, sock);
    user_set_state(username, U_AVAILABLE);
    users_set_opponent(username, "");
    if (current_user && user_cap) {
        strncpy(current_user, username, user_cap - 1);
        current_user[user_cap - 1] = '\0';
    }
    send_txt(sock, "Connexion réussie !\n");
    printf("Nouvel utilisateur : %s (total = %d)\n", username, get_user_count());
}

void cmd_LIST(int sock) {
    char listbuf[1024];
    get_user_list(listbuf, sizeof(listbuf));
    send_txt(sock, listbuf);
}

void cmd_CHALLENGE(int sock, const char *current_user, const char *buffer) {
    char target[64] = {0};
    if (sscanf(buffer, "%*s %63s", target) != 1) {
        send_txt(sock, "ERREUR : pseudo cible manquant\n");
        return;
    }
    char me[64]; me[0]='\0';
    if (current_user) { strncpy(me, current_user, 63); me[63]='\0'; }
    trim_inplace((char*)me);
    trim_inplace(target);

    if (me[0] == '\0') {
        send_txt(sock, "ERREUR : connecte-toi d'abord (LOGIN)\n");
        return;
    }
    if (!strcmp(me, target)) {
        send_txt(sock, "ERREUR : tu ne peux pas te défier toi-même\n");
        return;
    }
    if (!username_exists(target)) {
        send_txt(sock, "ERREUR : joueur introuvable\n");
        return;
    }
    int tsock = get_user_socket(target);
    if (tsock < 0 || fcntl(tsock, F_GETFD) == -1) {
        send_txt(sock, "ERREUR : joueur non disponible\n");
        return;
    }
    if (!user_is_available(me)) {
        send_txt(sock, "ERREUR : tu es occupé\n");
        return;
    }
    if (!user_is_available(target)) {
        send_txt(sock, "ERREUR : joueur occupé\n");
        return;
    }
    if (user_set_pending_pair(me, target) < 0) {
        send_txt(sock, "ERREUR : état changé, réessaie\n");
        return;
    }
    sendf(tsock, "CHALLENGE_FROM %s\n", me);
    send_txt(sock, "CHALLENGE_SENT\n");
}

void cmd_ACCEPT(int sock, const char *current_user, const char *buffer) {
    char opponent[64] = {0};
    if (sscanf(buffer, "%*s %63s", opponent) != 1) {
        send_txt(sock, "ERREUR : pseudo adversaire manquant\n");
        return;
    }
    if (!strcmp(current_user, opponent)) {
        send_txt(sock, "ERREUR : tu ne peux pas te défier toi-même\n");
        return;
    }
    if (current_user[0] == '\0') {
        send_txt(sock, "ERREUR : connecte-toi d'abord (LOGIN)\n");
        return;
    }
    int osock = get_user_socket(opponent);
    if (osock < 0) {
        send_txt(sock, "ERREUR : adversaire indisponible\n");
        return;
    }
    sendf(sock,  "CHALLENGE_ACCEPTED %s %s\n", opponent, current_user);
    sendf(osock, "CHALLENGE_ACCEPTED %s %s\n", opponent, current_user);
    int idx = game_new(opponent, osock, current_user, sock);
    if (idx < 0) {
        send_txt(sock, "ERREUR : trop de parties\n");
        return;
    }
    user_set_in_game_pair(opponent, current_user);
    if (rand() % 2) {
        Game g = games[idx];
        int sa = g.sock_a; g.sock_a = g.sock_b; g.sock_b = sa;
        char tmpn[64]; strncpy(tmpn, g.a, 63); tmpn[63]='\0';
        strncpy(g.a, g.b, 63); g.a[63]='\0';
        strncpy(g.b, tmpn, 63); g.b[63]='\0';
        games[idx] = g;
        aw_init(games[idx].board, &games[idx].p1, &games[idx].p2, &games[idx].player);
    }
    sendf(games[idx].sock_a, "GAME_START %s %s | STARTS %s\n",
          games[idx].a, games[idx].b,
          (games[idx].player==1)?games[idx].a:games[idx].b);
    sendf(games[idx].sock_b, "GAME_START %s %s | STARTS %s\n",
          games[idx].a, games[idx].b,
          (games[idx].player==1)?games[idx].a:games[idx].b);
    game_send_state(idx);
}

void cmd_REFUSE(int sock, const char *current_user, const char *buffer) {
    char opponent[64] = {0};
    if (sscanf(buffer, "%*s %63s", opponent) != 1) {
        send_txt(sock, "ERREUR : pseudo adversaire manquant\n");
        return;
    }
    if (current_user[0] == '\0') {
        send_txt(sock, "ERREUR : connecte-toi d'abord (LOGIN)\n");
        return;
    }
    trim_inplace(opponent);
    user_clear_pair(current_user, opponent);
    int osock = get_user_socket(opponent);
    if (osock >= 0) {
        sendf(osock, "CHALLENGE_REFUSED %s %s\n", opponent, current_user);
    }
    send_txt(sock, "REFUSE_OK\n");
}

void cmd_MOVE(int sock, const char *current_user, const char *buffer) {
    int k = 0;
    if (sscanf(buffer, "%*s %d", &k) != 1) {
        send_txt(sock, "ERREUR : MOVE <1..6>\n");
        return;
    }
    if (current_user[0] == '\0') {
        send_txt(sock, "ERREUR : connecte-toi d'abord (LOGIN)\n");
        return;
    }
    int gi = game_find_by_user(current_user);
    if (gi < 0 || !games[gi].active) {
        send_txt(sock, "ERREUR : aucune partie en cours\n");
        return;
    }
    int isA = (strcmp(games[gi].a, current_user) == 0);
    int my_player = isA ? 1 : 2;
    if (games[gi].player != my_player) {
        send_txt(sock, "ERREUR : pas ton tour\n");
        return;
    }

    int pit_index = -1;
    if (k >= 1 && k <= 6) {
        pit_index = (my_player == 1) ? (k-1) : (5+k);
    }
    if (pit_index < 0) {
        send_txt(sock, "ERREUR : MOVE invalide (1..6)\n");
        return;
    }
    if (!aw_is_legal(games[gi].board, games[gi].player, pit_index)) {
        send_txt(sock, "ERREUR : coup illégal\n");
        return;
    }

    int ended = aw_play(games[gi].board, &games[gi].p1, &games[gi].p2, &games[gi].player, pit_index);
    game_send_state(gi);

    if (ended) {
        const char *winner = (games[gi].p1 > games[gi].p2) ? games[gi].a :
                             (games[gi].p2 > games[gi].p1) ? games[gi].b : "DRAW";
        sendf(games[gi].sock_a, "GAME_OVER WINNER %s | SCORE %d %d\n",
              winner, games[gi].p1, games[gi].p2);
        sendf(games[gi].sock_b, "GAME_OVER WINNER %s | SCORE %d %d\n",
              winner, games[gi].p1, games[gi].p2);
        save_game_to_csv(games[gi].a, games[gi].b, games[gi].p1, games[gi].p2, winner);
        user_clear_pair(games[gi].a, games[gi].b);
        game_end(gi);
    }
}

void cmd_GAMES(int sock) {
    char out[2048]; out[0]='\0';
    strncat(out, "PARTIES:\n", sizeof(out)-1);
    pthread_mutex_lock(&games_mutex);
    for (int i=0;i<MAX_GAMES;i++) {
        if (!games[i].active) continue;
        char line[256];
        snprintf(line, sizeof(line), "  - id=%d : %s vs %s  (score %d:%d)\n",
                 games[i].id, games[i].a, games[i].b, games[i].p1, games[i].p2);
        strncat(out, line, sizeof(out)-strlen(out)-1);
    }
    pthread_mutex_unlock(&games_mutex);
    send_txt(sock, out);
}

void cmd_OBSERVE(int sock, const char *current_user, const char *buffer) {
    char arg[64] = {0};
    if (sscanf(buffer, "%*s %63s", arg) != 1) {
        send_txt(sock, "ERREUR : usage OBSERVE <id|pseudo>\n");
        return;
    }
    int gi = -1;
    char *endp = NULL;
    long id = strtol(arg, &endp, 10);
    if (endp && *endp == '\0') gi = game_find_by_id((int)id);
    else                       gi = game_find_by_user(arg);

    if (gi < 0 || !games[gi].active) {
        send_txt(sock, "ERREUR : partie introuvable\n");
        return;
    }
    if (sock == games[gi].sock_a || sock == games[gi].sock_b) {
        send_txt(sock, "INFO : tu es déjà joueur de cette partie\n");
        return;
    }
    int privateA = is_user_private(games[gi].a);
    int privateB = is_user_private(games[gi].b);
    if ((privateA && !is_friend_allowed(games[gi].a, current_user)) ||
        (privateB && !is_friend_allowed(games[gi].b, current_user))) {
        send_txt(sock, "INFO : cette partie est privée, tu n'es pas autorisé(e) à observer.\n");
        return;
    }
    game_add_spectator(gi, sock);
    send_txt(sock, "OBSERVE_OK\n");
    game_send_state(gi);
}

void cmd_UNOBSERVE(int sock) {
    int removed = 0;
    pthread_mutex_lock(&games_mutex);
    for (int i=0;i<MAX_GAMES;i++) {
        if (!games[i].active) continue;
        int before = games[i].spec_count;
        game_remove_spectator(i, sock);
        if (games[i].spec_count != before) removed = 1;
    }
    pthread_mutex_unlock(&games_mutex);
    if (removed) send_txt(sock, "UNOBSERVE_OK\n");
    else         send_txt(sock, "INFO : tu n'observais aucune partie\n");
}

void cmd_MSG(int sock, const char *current_user, const char *buffer) {
    char target[64] = {0}, msgtext[900] = {0};
    if (sscanf(buffer, "%*s %63s %[^\n]", target, msgtext) < 2) {
        send_txt(sock, "ERREUR : usage MSG <pseudo> <message>\n");
        return;
    }
    if (current_user[0] == '\0') {
        send_txt(sock, "ERREUR : connecte-toi d'abord (LOGIN)\n");
        return;
    }
    int tsock = get_user_socket(target);
    if (tsock < 0) {
        send_txt(sock, "ERREUR : joueur introuvable\n");
        return;
    }
    char formatted[1024];
    snprintf(formatted, sizeof(formatted), "[Privé de %s] %s\n", current_user, msgtext);
    send_txt(tsock, formatted);
    send_txt(sock, "MSG_OK\n");
}

void cmd_BROADCAST(int sock, const char *current_user, const char *buffer) {
    char msgtext[900] = {0};
    if (sscanf(buffer, "%*s %[^\n]", msgtext) != 1) {
        send_txt(sock, "ERREUR : usage BROADCAST <message>\n");
        return;
    }
    if (current_user[0] == '\0') {
        send_txt(sock, "ERREUR : connecte-toi d'abord (LOGIN)\n");
        return;
    }
    int nb_sent = 0;
    int total = get_user_count();
    for (int i = 0; i < total; i++) {
        const char* uname = get_username_by_index(i);
        if (!uname) continue;
        int s = get_user_socket(uname);
        if (s >= 0 && s != sock) {
            char formatted[1024];
            snprintf(formatted, sizeof(formatted), "[%s à tous] %s\n", current_user, msgtext);
            send_txt(s, formatted);
            nb_sent++;
        }
    }
    if (nb_sent == 0) send_txt(sock, "INFO : aucun destinataire connecté.\n");
    else              send_txt(sock, "BROADCAST_OK\n");
}

void cmd_SETBIO(int sock, const char *current_user, const char *buffer) {
    char bio[1024] = {0};
    if (sscanf(buffer, "%*s %[^\n]", bio) != 1) {
        send_txt(sock, "ERREUR : usage SETBIO <texte>\n");
        return;
    }
    if (current_user[0] == '\0') {
        send_txt(sock, "ERREUR : connecte-toi d'abord (LOGIN)\n");
        return;
    }
    set_user_bio(current_user, bio);
    send_txt(sock, "BIO_OK\n");
}

void cmd_SHOWBIO(int sock, const char *buffer) {
    char target[64] = {0};
    if (sscanf(buffer, "%*s %63s", target) != 1) {
        send_txt(sock, "ERREUR : usage SHOWBIO <pseudo>\n");
        return;
    }
    const char *bio = get_user_bio(target);
    if (!bio || bio[0] == '\0') {
        send_txt(sock, "Aucune bio trouvée pour cet utilisateur.\n");
    } else {
        char msg[1200];
        snprintf(msg, sizeof(msg), "--- Bio de %s ---\n%s\n-------------------\n", target, bio);
        send_txt(sock, msg);
    }
}

void cmd_ADDFRIEND(int sock, const char *current_user, const char *buffer) {
    char friendname[64] = {0};
    if (sscanf(buffer, "%*s %63s", friendname) != 1) {
        send_txt(sock, "ERREUR : usage ADDFRIEND <pseudo>\n");
        return;
    }
    if (current_user[0] == '\0') {
        send_txt(sock, "ERREUR : connecte-toi d'abord (LOGIN)\n");
        return;
    }
    if (!username_exists(friendname)) {
        send_txt(sock, "ERREUR : joueur inconnu\n");
        return;
    }
    add_user_friend(current_user, friendname);
    send_txt(sock, "FRIEND_ADDED\n");
}

void cmd_PRIVATEON(int sock, const char *current_user) {
    if (current_user[0] == '\0') {
        send_txt(sock, "ERREUR : connecte-toi d'abord (LOGIN)\n");
        return;
    }
    set_user_private(current_user, 1);
    send_txt(sock, "Mode privé activé : seuls tes amis peuvent observer tes parties.\n");
    remove_unallowed_spectators(current_user);
}

void cmd_PRIVATEOFF(int sock, const char *current_user) {
    if (current_user[0] == '\0') {
        send_txt(sock, "ERREUR : connecte-toi d'abord (LOGIN)\n");
        return;
    }
    set_user_private(current_user, 0);
    send_txt(sock, "Mode privé désactivé : tout le monde peut observer tes parties.\n");
}

void cmd_HISTORY(int sock) {
    FILE *f = fopen("data/games_history.csv", "r");
    if (!f) {
        send_txt(sock, "Aucune partie enregistrée.\n");
        return;
    }
    char line[256];
    send_txt(sock, "=== HISTORIQUE DES PARTIES ===\n");
    while (fgets(line, sizeof(line), f)) {
        send_txt(sock, line);
    }
    fclose(f);
    send_txt(sock, "===============================\n");
}
