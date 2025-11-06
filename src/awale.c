// awale.c : implémentation minimale des règles (sans aff/SDL)
#include "awale.h"
#include <string.h>
// games.c


// règle simple : score >= 25 => fin
static int game_over(const int board[12], int p1, int p2) {
    // condition 1 : score >= 25
    if (p1 >= 25 || p2 >= 25)
        return 1;

    // condition 2 : il ne reste que deux cases contenant exactement 1 graine
    int count_ones = 0;
    int non_empty = 0;
    for (int i = 0; i < 12; i++) {
        if (board[i] > 0) {
            non_empty++;
            if (board[i] == 1) count_ones++;
        }
    }

    // Fin de partie si toutes les cases sont vides sauf deux avec 1 graine chacune
    if (non_empty == 2 && count_ones == 2)
        return 1;

    return 0;
}


void aw_init(int board[12], int *p1, int *p2, int *player) {
    for (int i = 0; i < 12; i++) board[i] = 4;
    *p1 = 0; *p2 = 0; *player = 1;
}

int aw_is_legal(const int board[12], int player, int pit) {
    // pit : 0..11
    if (pit < 0 || pit > 11) return 0;
    if (board[pit] <= 0) return 0;
    if (player == 1) {
        return (pit >= 0 && pit <= 5);
    } else {
        return (pit >= 6 && pit <= 11);
    }
}


int aw_play(int board[12], int *p1, int *p2, int *player, int pit) {
    int n = board[pit];
    board[pit] = 0;

    int cur = pit;
    for (int i = 0; i < n; i++) {
        cur = (cur + 1) % 12;
        board[cur] += 1;
    }

    // capture
    int gained = 0;
    if (board[cur] == 2 || board[cur] == 3) {
        gained += board[cur];
        board[cur] = 0;

        // reculer sur les cases précédentes du semis
        int back = cur;
        for (int i = 0; i < n; i++) {
            back = (back - 1 + 12) % 12;
            if (board[back] == 2 || board[back] == 3) {
                gained += board[back];
                board[back] = 0;
            } else {
                break; // on s’arrête dès que ce n’est plus 2/3
            }
        }
    }

    if (*player == 1) *p1 += gained; else *p2 += gained;

    // change de joueur
    *player = (*player == 1) ? 2 : 1;

    return game_over(board,*p1, *p2);
}
