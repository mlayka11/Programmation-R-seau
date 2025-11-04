#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>

#define PORT 8080

/* Supprime le '\n' final si présent */
void remove_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n')
        str[len - 1] = '\0';
}

/* Connexion TCP au serveur */
int connect_to_server(void) {
    int sock;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Erreur : création de la socket\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Adresse invalide / non supportée\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connexion échouée\n");
        return -1;
    }

    printf("Connecté au serveur (port %d)\n", PORT);
    return sock;
}

/* Login utilisateur */
int login_user(int sock, char *username) {
    char buffer[1024] = {0};
    char message[128];

    printf("Entre ton pseudo (20 caractères max, lettres, chiffres, - ou _) : ");
    if (fgets(username, 64, stdin) == NULL) {
        printf("Erreur de lecture du pseudo\n");
        return 0;
    }

    remove_newline(username);
    snprintf(message, sizeof(message), "LOGIN %s\n", username);

    send(sock, message, strlen(message), 0);
    printf("Connexion en cours avec le pseudo : %s\n", username);

    int valread = read(sock, buffer, sizeof(buffer) - 1);
    if (valread > 0) {
        buffer[valread] = '\0';
        printf("Réponse du serveur : %s\n", buffer);
        return strstr(buffer, "Connexion réussie") != NULL;
    }
    return 0;
}

/* Boucle principale de commandes */
void command_loop(int sock) {
    char buffer[1024];
    char command[128];

    printf("\nVous êtes maintenant connecté(e) !\n");
    printf("Commandes disponibles :\n");
    printf("   - LIST                       : voir les utilisateurs connectés\n");
    printf("   - CHALLENGE <pseudo>         : défier un joueur\n");
    printf("   - ACCEPT <pseudo>            : accepter un défi\n");
    printf("   - REFUSE <pseudo>            : refuser un défi\n");
    printf("   - MOVE <1..6>                : jouer un coup (pendant une partie)\n");
    printf("   - GAMES                      : voir les parties en cours\n");
    printf("   - OBSERVE <id|pseudo>        : observer une partie\n");
    printf("   - UNOBSERVE                  : arrêter d'observer\n");
    printf("   - QUIT                       : quitter\n\n");
    printf("   - MSG <pseudo> msg           : message privé\n\n");
    printf("    - BROADCAST msg              : message public\n\n");

    while (1) {
        fd_set readfds;
        int maxfd;

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        maxfd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

        printf("> ");
        fflush(stdout);

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        /* Message reçu du serveur */
        if (FD_ISSET(sock, &readfds)) {
            int n = read(sock, buffer, sizeof(buffer) - 1);
            if (n <= 0) {
                printf("\nLe serveur s'est déconnecté.\n");
                break;
            }
            buffer[n] = '\0';
            printf("\n[Serveur] %s", buffer);
        }

        /* Commande saisie par l'utilisateur */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(command, sizeof(command), stdin) == NULL)
                break;

            remove_newline(command);

            if (strcmp(command, "QUIT") == 0) {
                const char *bye = "QUIT\n";
                send(sock, bye, strlen(bye), 0);
                printf("Déconnexion en cours...\n");
                break;
            }

            if (command[0] == '\0')
                continue;

            strcat(command, "\n");
            send(sock, command, strlen(command), 0);
        }
    }
}

/* Programme principal */
int main(void) {
    int sock;
    char username[64];

    printf("===========================================\n");
    printf(" Bienvenue sur le serveur Awalé en ligne\n");
    printf("===========================================\n\n");

    sock = connect_to_server();
    if (sock < 0)
        return -1;

    if (login_user(sock, username))
        command_loop(sock);

    close(sock);
    return 0;
}
