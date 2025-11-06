#ifndef GAMES_H
#define GAMES_H

#include <pthread.h>
#include "awale.h"
#include "users.h"

#define MAX_GAMES 128
#define MAX_SPECTATORS 16

typedef struct {
    int active;
    int id;
    char a[64], b[64];
    int sock_a, sock_b;
    int board[12];
    int p1, p2;
    int player;
    int specs[MAX_SPECTATORS];
    int spec_count;
} Game;


void games_init(void);
int game_new(const char *a, int sock_a, const char *b, int sock_b);
void game_end(int idx);
int game_find_by_user(const char *user);
int game_find_by_socket(int sock);
int game_find_by_id(int id);
void game_abort_by_socket(int sock);
void game_add_spectator(int idx, int sock);
void game_remove_spectator(int idx, int sock);
void game_send_state(int idx);
void remove_unallowed_spectators(const char *username) ;
void save_game_to_csv(const char *playerA, const char *playerB,int scoreA, int scoreB, const char *winner);
extern pthread_mutex_t games_mutex;
extern Game games[MAX_GAMES];

#endif
