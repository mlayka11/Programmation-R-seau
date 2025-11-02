#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>

int gainPlayer1 = 0;
int gainPlayer2 = 0;

void init(int tab[12]) {
    for (int i = 0; i < 12; i++) {
        tab[i] = 4;
    }
}

void drawBoard(int tab[12], int player, int g1, int g2) {
    clear();
    mvprintw(1, 5, "=== AWALE ===");
    mvprintw(3, 5, "Score J1: %d    Score J2: %d", g1, g2);
    mvprintw(4, 5, "Tour du joueur %d", player);
    // Ligne supérieure (joueur 2)
    mvprintw(6, 5, "J2 -> ");
    for (int i = 11; i >= 6; i--) {
        mvprintw(6, 12 + (11 - i) * 5, "[%2d]", tab[i]);
    }
    // Ligne inférieure (joueur 1)
    mvprintw(8, 5, "J1 -> ");
    for (int i = 0; i < 6; i++) {
        mvprintw(8, 12 + i * 5, "[%2d]", tab[i]);
    }

    mvprintw(10, 5, "Choisissez une case (1-6) ou 'q' pour quitter : ");
    refresh();
}

int game(int tab[12], int playerNumber) {
    int gain = 0;
    int pit;
    char input[10];

    while (1) {
        drawBoard(tab, playerNumber, gainPlayer1, gainPlayer2);
        echo();
        getnstr(input, 9);
        noecho();

        if (input[0] == 'q') {
            endwin();
            exit(0);
        }

        pit = atoi(input);
        if (pit < 1 || pit > 6) continue;
        if ((playerNumber == 1 && tab[pit - 1] == 0) ||
            (playerNumber == 2 && tab[pit + 5] == 0))
            continue;
        break;
    }

    if (playerNumber == 2) pit += 6;

    int n = tab[pit - 1];
    tab[pit - 1] = 0;

    for (int i = 0; i < n; i++) {
        pit = pit % 12;
        tab[pit]++;
        pit++;
    }

    pit--;
    if (tab[pit] == 2 || tab[pit] == 3) {
        gain += tab[pit];
        tab[pit] = 0;

        for (int i = 0; i < n; i++) {
            if (pit < 0) pit = 11;
            if (tab[pit] == 2 || tab[pit] == 3) {
                gain += tab[pit];
                tab[pit] = 0;
            }
            pit--;
        }
    }

    return gain;
}

int main() {
    int board[12];
    init(board);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    int round = 1;
    while (gainPlayer1 < 25 && gainPlayer2 < 25) {
        if (round % 2)
            gainPlayer1 += game(board, 1);
        else
            gainPlayer2 += game(board, 2);
        round++;
    }

    clear();
    mvprintw(5, 10, "=== FIN DE PARTIE ===");
    mvprintw(7, 10, "Score J1: %d   Score J2: %d", gainPlayer1, gainPlayer2);
    if (gainPlayer1 > gainPlayer2)
        mvprintw(9, 10, ">> Joueur 1 gagne !");
    else if (gainPlayer2 > gainPlayer1)
        mvprintw(9, 10, ">> Joueur 2 gagne !");
    else
        mvprintw(9, 10, ">> Égalité !");
    mvprintw(11, 10, "Appuyez sur une touche pour quitter...");
    getch();

    endwin();
    return 0;
}
