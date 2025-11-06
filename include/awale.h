
#ifndef AWALE_H
#define AWALE_H


void aw_init(int board[12], int *p1, int *p2, int *player);

int aw_is_legal(const int board[12], int player, int pit_index);

int aw_play(int board[12], int *p1, int *p2, int *player, int pit_index);

#endif
