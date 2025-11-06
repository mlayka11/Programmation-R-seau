#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include "users.h"


#define MAX_FRIENDS 16
static char user_friends[100][MAX_FRIENDS][64]; // 100 users max
static int user_friend_count[100];
static int user_private_mode[100];  // 0 = public, 1 = privé


/* Mutex global pour protéger la liste d'utilisateurs */
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Déclaration vers le module jeu (pour gérer déconnexion en partie) */
extern void game_abort_by_socket(int sock);

/* Données internes */
static char **users = NULL;
static int *user_sockets = NULL;
static user_state_t *user_states = NULL;
static char (*user_opponent)[64] = NULL;
static int user_count = 0;
static int user_capacity = 0;

/* --- Fonctions internes --- */
static int index_by_name(const char *u) {
    for (int i = 0; i < user_count; i++)
        if (users[i] && strcmp(users[i], u) == 0)
            return i;
    return -1;
}

static int index_by_socket(int sock) {
    for (int i = 0; i < user_count; i++)
        if (user_sockets[i] == sock)
            return i;
    return -1;
}

/* --- Initialisation et nettoyage --- */
/* ---- Ajout : stockage des bios ---- */
static char (*user_bio)[1024] = NULL;

/* Mets à jour init_users() */
void init_users(int initial_capacity) {
    user_capacity = (initial_capacity > 0) ? initial_capacity : 10;

    users         = (char**)calloc(user_capacity, sizeof(char*));
    user_sockets  = (int*)calloc(user_capacity, sizeof(int));
    user_states   = (user_state_t*)calloc(user_capacity, sizeof(user_state_t));
    user_opponent = (char (*)[64])calloc(user_capacity, sizeof(*user_opponent));
    user_bio      = (char (*)[1024])calloc(user_capacity, sizeof(*user_bio));

    for (int i = 0; i < user_capacity; i++) {
        user_sockets[i] = -1;
        user_states[i]  = U_OFFLINE;
        user_opponent[i][0] = '\0';
        user_bio[i][0] = '\0';
        user_private_mode[i] = 0;
    }
    user_count = 0;
}

void set_user_bio(const char *username, const char *bio) {
    pthread_mutex_lock(&users_mutex);
    int i = -1;
    for (int j = 0; j < user_count; j++) {
        if (users[j] && strcmp(users[j], username) == 0) { i = j; break; }
    }
    if (i >= 0) {
        strncpy(user_bio[i], bio, sizeof(user_bio[i]) - 1);
        user_bio[i][sizeof(user_bio[i]) - 1] = '\0';
    }
    pthread_mutex_unlock(&users_mutex);
}

const char* get_user_bio(const char *username) {
    pthread_mutex_lock(&users_mutex);
    int i = -1;
    for (int j = 0; j < user_count; j++) {
        if (users[j] && strcmp(users[j], username) == 0) { i = j; break; }
    }
    const char *res = (i >= 0) ? user_bio[i] : NULL;
    pthread_mutex_unlock(&users_mutex);
    return res;
}


static void ensure_capacity(void) {
    if (user_count < user_capacity) return;

    int old = user_capacity;
    int new_cap = user_capacity * 2;
    users = realloc(users, new_cap * sizeof(char*));
    user_sockets = realloc(user_sockets, new_cap * sizeof(int));
    user_states = realloc(user_states, new_cap * sizeof(user_state_t));
    user_opponent = realloc(user_opponent, new_cap * sizeof(*user_opponent));

    for (int i = old; i < new_cap; i++) {
        users[i] = NULL;
        user_sockets[i] = -1;
        user_states[i] = U_OFFLINE;
        user_opponent[i][0] = '\0';
    }
    user_capacity = new_cap;
}

void free_users(void) {
    for (int i = 0; i < user_count; i++) {
        free(users[i]);
    }
    free(users);
    free(user_sockets);
    free(user_states);
    free(user_opponent);
    users = NULL;
    user_sockets = NULL;
    user_states = NULL;
    user_opponent = NULL;
    user_count = 0;
    user_capacity = 0;
}

/* --- Vérifications et utilitaires --- */
int is_valid_username(const char *u) {
    int len = strlen(u);
    if (len == 0 || len > 20) return 0;
    for (int i = 0; i < len; i++) {
        char c = u[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

int username_exists(const char *u) {
    for (int i = 0; i < user_count; i++) {
        if (users[i] && strcmp(users[i], u) == 0) {
            if (user_states[i] == U_AVAILABLE || user_states[i] == U_PENDING || user_states[i] == U_IN_GAME)
                return 1;
            else
                return 0; 
        }
    }
    return 0;
}


int get_user_count(void) {
    return user_count;
}

void get_user_list(char *buffer, int bufsize) {
    if (bufsize <= 0) return;

    buffer[0] = '\0';
    strncat(buffer, "UTILISATEURS :", (size_t)bufsize - 1);

    for (int i = 0; i < user_count; i++) {
        if (!users[i]) continue;
        if (user_sockets[i] < 0) continue;
        size_t used = strlen(buffer);
        if (used + 1 >= (size_t)bufsize) break;
        strncat(buffer, " ", (size_t)bufsize - used - 1);

        used = strlen(buffer);
        if (used >= (size_t)bufsize - 1) break;
        strncat(buffer, users[i], (size_t)bufsize - used - 1);
    }

    size_t used = strlen(buffer);
    if (used < (size_t)bufsize - 1) {
        strncat(buffer, "\n", (size_t)bufsize - used - 1);
    }
}


/* --- Gestion des utilisateurs --- */
int add_user(const char *u) {
    pthread_mutex_lock(&users_mutex);
    ensure_capacity();
    users[user_count] = malloc(strlen(u) + 1);
    strcpy(users[user_count], u);
    user_sockets[user_count] = -1;
    user_states[user_count] = U_OFFLINE;
    user_opponent[user_count][0] = '\0';
    user_count++;
    pthread_mutex_unlock(&users_mutex);
    return 1;
}

int remove_user(const char *u) {
    pthread_mutex_lock(&users_mutex);
    int i = index_by_name(u);
    if (i < 0) {
        pthread_mutex_unlock(&users_mutex);
        return 0;
    }
    free(users[i]);
    users[i] = users[user_count - 1];
    user_sockets[i] = user_sockets[user_count - 1];
    user_states[i] = user_states[user_count - 1];
    strcpy(user_opponent[i], user_opponent[user_count - 1]);
    users[user_count - 1] = NULL;
    user_sockets[user_count - 1] = -1;
    user_states[user_count - 1] = U_OFFLINE;
    user_opponent[user_count - 1][0] = '\0';
    user_count--;
    pthread_mutex_unlock(&users_mutex);
    return 1;
}

/* --- Sockets et mapping --- */
int set_user_socket(const char *u, int sock) {
    pthread_mutex_lock(&users_mutex);
    int i = index_by_name(u);
    if (i >= 0) user_sockets[i] = sock;
    pthread_mutex_unlock(&users_mutex);
    return (i >= 0);
}

int get_user_socket(const char *u) {
    pthread_mutex_lock(&users_mutex);
    int i = index_by_name(u);
    int s = (i >= 0) ? user_sockets[i] : -1;
    pthread_mutex_unlock(&users_mutex);
    return s;
}

const char *get_username_by_socket(int sock) {
    pthread_mutex_lock(&users_mutex);
    int i = index_by_socket(sock);
    const char *res = (i >= 0) ? users[i] : NULL;
    pthread_mutex_unlock(&users_mutex);
    return res;
}

const char *get_username_by_index(int i) {
    if (i < 0 || i >= user_count) return NULL;
    return users[i];
}

/* --- États et adversaires --- */
void user_set_state(const char *name, user_state_t st) {
    pthread_mutex_lock(&users_mutex);
    int i = index_by_name(name);
    if (i >= 0) user_states[i] = st;
    pthread_mutex_unlock(&users_mutex);
}

void users_set_opponent(const char *name, const char *opp) {
    pthread_mutex_lock(&users_mutex);
    int i = index_by_name(name);
    if (i >= 0)
        strncpy(user_opponent[i], opp, sizeof(user_opponent[i]) - 1);
    pthread_mutex_unlock(&users_mutex);
}

int user_is_available(const char *name) {
    pthread_mutex_lock(&users_mutex);
    int i = index_by_name(name);
    int ok = (i >= 0 && user_sockets[i] >= 0 && user_states[i] == U_AVAILABLE);
    pthread_mutex_unlock(&users_mutex);
    return ok;
}

int user_set_pending_pair(const char *a, const char *b) {
    int rc = -1;
    pthread_mutex_lock(&users_mutex);
    int ia = index_by_name(a);
    int ib = index_by_name(b);
    if (ia >= 0 && ib >= 0 &&
        user_states[ia] == U_AVAILABLE && user_states[ib] == U_AVAILABLE) {
        user_states[ia] = U_PENDING; strncpy(user_opponent[ia], b, 63);
        user_states[ib] = U_PENDING; strncpy(user_opponent[ib], a, 63);
        rc = 0;
    }
    pthread_mutex_unlock(&users_mutex);
    return rc;
}

void user_set_in_game_pair(const char *a, const char *b) {
    pthread_mutex_lock(&users_mutex);
    int ia = index_by_name(a);
    int ib = index_by_name(b);
    if (ia >= 0) { user_states[ia] = U_IN_GAME; strncpy(user_opponent[ia], b, 63); }
    if (ib >= 0) { user_states[ib] = U_IN_GAME; strncpy(user_opponent[ib], a, 63); }
    pthread_mutex_unlock(&users_mutex);
}

void user_clear_pair(const char *a, const char *b) {
    pthread_mutex_lock(&users_mutex);
    int ia = index_by_name(a);
    int ib = index_by_name(b);
    if (ia >= 0) { user_states[ia] = U_AVAILABLE; user_opponent[ia][0] = '\0'; }
    if (ib >= 0) { user_states[ib] = U_AVAILABLE; user_opponent[ib][0] = '\0'; }
    pthread_mutex_unlock(&users_mutex);
}

/* --- Déconnexion propre --- */
void set_socket_disconnected(int sock) {
    const char* me = get_username_by_socket(sock);

    if (me && me[0]) {
        printf(">> Déconnexion de %s\n", me);

        // Marquer l’utilisateur comme disponible à nouveau
        set_user_socket(me, -1);
        user_set_state(me, U_AVAILABLE);  // ← avant : U_OFFLINE
        users_set_opponent(me, "");

    } else {
        // Aucun pseudo connu, on nettoie juste le socket
        for (int i = 0; i < user_count; i++) {
            if (user_sockets[i] == sock) {
                user_sockets[i] = -1;
                break;
            }
        }
    }

    // Nettoyer toute partie associée
    game_abort_by_socket(sock);
}

void add_user_friend(const char *username, const char *friendname) {
    pthread_mutex_lock(&users_mutex);
    int i = -1;
    for (int j = 0; j < user_count; j++) {
        if (users[j] && strcmp(users[j], username) == 0) { i = j; break; }
    }
    if (i >= 0 && user_friend_count[i] < MAX_FRIENDS) {
        // Éviter les doublons
        for (int k = 0; k < user_friend_count[i]; k++) {
            if (strcmp(user_friends[i][k], friendname) == 0) {
                pthread_mutex_unlock(&users_mutex);
                return;
            }
        }
        strncpy(user_friends[i][user_friend_count[i]++], friendname, 63);
    }
    pthread_mutex_unlock(&users_mutex);
}

int is_friend_allowed(const char *username, const char *friendname) {
    pthread_mutex_lock(&users_mutex);
    int i = -1;
    for (int j = 0; j < user_count; j++) {
        if (users[j] && strcmp(users[j], username) == 0) { i = j; break; }
    }
    int ok = 0;
    if (i >= 0) {
        for (int k = 0; k < user_friend_count[i]; k++) {
            if (strcmp(user_friends[i][k], friendname) == 0) {
                ok = 1;
                break;
            }
        }
    }
    pthread_mutex_unlock(&users_mutex);
    return ok;
}

void set_user_private(const char *username, int mode) {
    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < user_count; i++) {
        if (users[i] && strcmp(users[i], username) == 0) {
            user_private_mode[i] = (mode != 0);
            break;
        }
    }
    pthread_mutex_unlock(&users_mutex);
}

int is_user_private(const char *username) {
    pthread_mutex_lock(&users_mutex);
    int res = 0;
    for (int i = 0; i < user_count; i++) {
        if (users[i] && strcmp(users[i], username) == 0) {
            res = user_private_mode[i];
            break;
        }
    }
    pthread_mutex_unlock(&users_mutex);
    return res;
}

