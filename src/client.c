#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdlib.h>

#define PORT 8000
#define CLEAR_SCREEN "\033[2J\033[H"
#define RESET_COLOR "\033[0m"
#define BLUE "\033[1;34m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define CYAN "\033[1;36m"
#define MAGENTA "\033[1;35m"
#define RED "\033[1;31m"
#define WHITE "\033[1;37m"

/* --- Fonctions utilitaires --- */
void remove_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n')
        str[len - 1] = '\0';
}

/* Connexion au serveur */
int connect_to_server(void) {
    int sock;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf(RED "Erreur : création de la socket\n" RESET_COLOR);
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf(RED "Adresse invalide / non supportée\n" RESET_COLOR);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf(RED "Connexion échouée\n" RESET_COLOR);
        return -1;
    }

    printf(GREEN "Connecté au serveur (port %d)\n" RESET_COLOR, PORT);
    return sock;
}

/* Authentification utilisateur */
int login_user(int sock, char *username) {
    char buffer[1024] = {0};
    char message[128];

    printf("Entre ton pseudo (20 caractères max) : ");
    if (fgets(username, 64, stdin) == NULL) {
        printf(RED "Erreur de lecture du pseudo\n" RESET_COLOR);
        return 0;
    }

    remove_newline(username);
    snprintf(message, sizeof(message), "LOGIN %s\n", username);

    send(sock, message, strlen(message), 0);
    printf("Connexion en cours avec le pseudo : %s\n", username);

    int valread = read(sock, buffer, sizeof(buffer) - 1);
    if (valread > 0) {
        buffer[valread] = '\0';
        printf("%s\n", buffer);
        return strstr(buffer, "Connexion réussie") != NULL;
    }
    return 0;
}

/* --- Boucle principale --- */
void command_loop(int sock, const char *username) {
    char buffer[2048];
    char command[256];
    fd_set readfds;
    int maxfd;

    printf(CLEAR_SCREEN);
    printf(GREEN "===========================================\n");
    printf(" Connecté en tant que : %s\n", username);
    printf("===========================================\n" RESET_COLOR);

    printf(MAGENTA "\nCommandes disponibles :\n" RESET_COLOR);
    printf("  " CYAN "LIST" RESET_COLOR "          → voir les utilisateurs connectés\n");
    printf("  " CYAN "CHALLENGE <pseudo>" RESET_COLOR " → défier un joueur\n");
    printf("  " CYAN "ACCEPT <pseudo>" RESET_COLOR "    → accepter un défi\n");
    printf("  " CYAN "MOVE <1..6>" RESET_COLOR "        → jouer un coup\n");
    printf("  " CYAN "MSG <pseudo> <msg>" RESET_COLOR " → envoyer un message privé\n");
    printf("  " CYAN "BROADCAST <msg>" RESET_COLOR "    → envoyer un message à tous\n");
    printf("  " CYAN "SETBIO <texte>" RESET_COLOR "     → définir ta biographie\n");
    printf("  " CYAN "SHOWBIO <pseudo>" RESET_COLOR "   → afficher une biographie\n");
    printf("  " CYAN "PRIVATEON / PRIVATEOFF" RESET_COLOR " → activer / désactiver le mode privé\n");
    printf("  " CYAN "HISTORY" RESET_COLOR "            → voir les parties terminées\n");
    printf("  " CYAN "QUIT" RESET_COLOR "               → quitter le serveur\n");
    printf(GREEN "------------------------------------------------------------\n" RESET_COLOR);

    printf(GREEN "> " RESET_COLOR);
    fflush(stdout);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        maxfd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        /* === Message reçu du serveur === */
        if (FD_ISSET(sock, &readfds)) {
            int n = read(sock, buffer, sizeof(buffer) - 1);
            if (n <= 0) {
                printf(RED "\nLe serveur s'est déconnecté.\n" RESET_COLOR);
                break;
            }

            buffer[n] = '\0';
            printf("\n");

            if (strstr(buffer, "[CHAT]")) {
                printf(CYAN "[Message] %s\n" RESET_COLOR, buffer);
            } else if (strstr(buffer, "[GAME]")) {
                printf(YELLOW "[Jeu] %s\n" RESET_COLOR, buffer);
            } else if (strstr(buffer, "[BROADCAST]")) {
                printf(MAGENTA "[Broadcast] %s\n" RESET_COLOR, buffer);
            } else if (strstr(buffer, "Connexion réussie")) {
                printf(GREEN "%s\n" RESET_COLOR, buffer);
            } else if (strstr(buffer, "ERREUR")) {
                printf(RED "%s\n" RESET_COLOR, buffer);
            } else {
                printf(WHITE "%s\n" RESET_COLOR, buffer);
            }

            printf(GREEN "> " RESET_COLOR);
            fflush(stdout);
        }

        /* === Commande utilisateur === */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(command, sizeof(command), stdin) == NULL)
                break;

            remove_newline(command);

            // Ignorer les lignes vides
            if (strlen(command) == 0) {
                printf(GREEN "> " RESET_COLOR);
                fflush(stdout);
                continue;
            }

            // Commande QUIT
            if (strcmp(command, "QUIT") == 0) {
                const char *bye = "QUIT\n";
                send(sock, bye, strlen(bye), 0);
                printf(RED "\nDéconnexion en cours...\n" RESET_COLOR);
                break;
            }

            // Envoi de la commande au serveur
            strcat(command, "\n");
            send(sock, command, strlen(command), 0);

            printf(GREEN "> " RESET_COLOR);
            fflush(stdout);
        }
    }
}

/* --- Main --- */
int main(void) {
    int sock;
    char username[64];

    printf(CLEAR_SCREEN);
    printf(GREEN "===========================================\n");
    printf(" Bienvenue sur le serveur Awalé en ligne\n");
    printf("===========================================\n\n" RESET_COLOR);

    sock = connect_to_server();
    if (sock < 0)
        return -1;

    if (login_user(sock, username))
        command_loop(sock, username);

    close(sock);
    return 0;
}
