#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "users.h"

static char **users = NULL;
static int user_count = 0;
static int user_capacity = 0;
static int *user_sockets = NULL;

/* Mutex global pour éviter les accès concurrents */
static pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_users(int initial_capacity) {
    pthread_mutex_lock(&users_mutex);

    if (initial_capacity > 0)
        user_capacity = initial_capacity;
    else
        user_capacity = 10;

    users = (char **)malloc(user_capacity * sizeof(char *));
    user_sockets = (int *)malloc(user_capacity * sizeof(int));

    for (int i = 0; i < user_capacity; i++) {
        users[i] = NULL;
        user_sockets[i] = -1;
    }

    user_count = 0;
    pthread_mutex_unlock(&users_mutex);
}

static void ensure_capacity(void) {
    if (user_count < user_capacity) return;

    int old_cap = user_capacity;
    int new_cap = (user_capacity > 0) ? user_capacity * 2 : 10;

    char **tmp = (char **)realloc(users, new_cap * sizeof(char *));
    int *tmp_sock = (int *)realloc(user_sockets, new_cap * sizeof(int));
    if (!tmp || !tmp_sock) return;

    users = tmp;
    user_sockets = tmp_sock;

    for (int i = old_cap; i < new_cap; i++) {
        users[i] = NULL;
        user_sockets[i] = -1;
    }

    user_capacity = new_cap;
}

void free_users(void) {
    pthread_mutex_lock(&users_mutex);

    if (users) {
        for (int i = 0; i < user_count; i++) {
            free(users[i]);
            users[i] = NULL;
        }
        free(users);
        users = NULL;
    }
    if (user_sockets) {
        free(user_sockets);
        user_sockets = NULL;
    }
    user_count = 0;
    user_capacity = 0;

    pthread_mutex_unlock(&users_mutex);
}

int username_exists(const char *u) {
    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < user_count; i++) {
        if (users[i] && strcmp(users[i], u) == 0) {
            pthread_mutex_unlock(&users_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&users_mutex);
    return 0;
}

int is_valid_username(const char* u) { 
    int len = (int)strlen(u); 
    if (len == 0 || len > 20) return 0; 
    for (int i = 0; i < len; i++) { 
        char c = u[i]; 
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0; 
    } 
    return 1; 
}

int get_user_count(void) {
    pthread_mutex_lock(&users_mutex);
    int count = user_count;
    pthread_mutex_unlock(&users_mutex);
    return count;
}

void get_user_list(char *buffer, int bufsize) {
    pthread_mutex_lock(&users_mutex);

    if (bufsize <= 0) {
        pthread_mutex_unlock(&users_mutex);
        return;
    }

    buffer[0] = '\0';
    strncat(buffer, "UTILISATEURS :", (size_t)bufsize - 1);

    for (int i = 0; i < user_count; i++) {
        if (!users[i] || user_sockets[i] == -1) continue;

        size_t used = strlen(buffer);
        if (used + 1 >= (size_t)bufsize) break;
        strncat(buffer, " ", (size_t)bufsize - used - 1);

        used = strlen(buffer);
        if (used >= (size_t)bufsize - 1) break;
        strncat(buffer, users[i], (size_t)bufsize - used - 1);
    }

    size_t used = strlen(buffer);
    if (used < (size_t)bufsize - 1)
        strncat(buffer, "\n", (size_t)bufsize - used - 1);

    pthread_mutex_unlock(&users_mutex);
}

int add_user(const char *u) {
    pthread_mutex_lock(&users_mutex);

    ensure_capacity();
    users[user_count] = (char*)malloc(strlen(u) + 1);
    strcpy(users[user_count], u);
    user_sockets[user_count] = -1;
    user_count++;

    pthread_mutex_unlock(&users_mutex);
    return 1;
}

int remove_user(const char *u) {
    if (!u) return 0;
    pthread_mutex_lock(&users_mutex);

    for (int i = 0; i < user_count; i++) {
        if (users[i] && strcmp(users[i], u) == 0) {
            free(users[i]);
            users[i] = users[user_count - 1];
            user_sockets[i] = user_sockets[user_count - 1];
            users[user_count - 1] = NULL;
            user_sockets[user_count - 1] = -1;
            user_count--;
            pthread_mutex_unlock(&users_mutex);
            return 1;
        }
    }

    pthread_mutex_unlock(&users_mutex);
    return 0;
}

/* Marque le socket comme déconnecté */
void set_socket_disconnected(int sock) {
    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < user_count; i++) {
        if (user_sockets[i] == sock) {
            user_sockets[i] = -1;
            break;
        }
    }
    pthread_mutex_unlock(&users_mutex);
}

int set_user_socket(const char *u, int sock) {
    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < user_count; i++) {
        if (users[i] && strcmp(users[i], u) == 0) {
            user_sockets[i] = sock;
            pthread_mutex_unlock(&users_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&users_mutex);
    return 0;
}

int get_user_socket(const char *u) {
    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < user_count; i++) {
        if (users[i] && strcmp(users[i], u) == 0) {
            int sock = user_sockets[i];
            pthread_mutex_unlock(&users_mutex);
            return sock;
        }
    }
    pthread_mutex_unlock(&users_mutex);
    return -1;
}

/* Recherche inverse : socket -> username */
const char* get_username_by_socket(int sock) {
    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < user_count; i++) {
        if (user_sockets[i] == sock) {
            const char* name = users[i];
            pthread_mutex_unlock(&users_mutex);
            return name;
        }
    }
    pthread_mutex_unlock(&users_mutex);
    return NULL;
}
