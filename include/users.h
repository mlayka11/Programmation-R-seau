#ifndef USERS_H
#define USERS_H

#include <stddef.h>
#include <pthread.h>

extern pthread_mutex_t users_mutex;

typedef enum {
    U_OFFLINE = 0,
    U_AVAILABLE,
    U_PENDING,
    U_IN_GAME
} user_state_t;

/* init / free */
void init_users(int initial_capacity);
void free_users(void);

/* CRUD */
int  add_user(const char *u);
int  remove_user(const char *u);
int  username_exists(const char *u);
int  is_valid_username(const char *u);

/* infos */
int  get_user_count(void);
void get_user_list(char *buffer, int bufsize);

/* sockets */
int  set_user_socket(const char *u, int sock);
int  get_user_socket(const char *u);
const char* get_username_by_socket(int sock);
void set_socket_disconnected(int sock);

/* états */
void user_set_state(const char *name, user_state_t st);
void users_set_opponent(const char *name, const char *opp);
int  user_is_available(const char *name);
int  user_set_pending_pair(const char *a, const char *b);   /* atomique, 0 = OK */
void user_set_in_game_pair(const char *a, const char *b);
void user_clear_pair(const char *a, const char *b);
const char* get_username_by_index(int i);
void set_user_bio(const char *username, const char *bio);
const char* get_user_bio(const char *username);
void add_user_friend(const char *username, const char *friendname);
int is_friend_allowed(const char *username, const char *friendname);
void set_user_private(const char *username, int mode);
int is_user_private(const char *username);





/* trim util si besoin côté serveur */
static inline void trim_inplace(char *s) {
    if (!s) return;
    char *p = s;
    while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') p++;
    if (p!=s) memmove(s,p,strlen(p)+1);
    size_t n = strlen(s);
    while (n>0 && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n')) s[--n] = '\0';
}

#endif
