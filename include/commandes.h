#ifndef COMMANDES_H
#define COMMANDES_H

#include <stddef.h>

/* API uniforme : chaque commande reçoit
   - sock          : le socket du client
   - current_user  : buffer modifiable contenant le pseudo courant ("" si non loggé)
   - user_cap      : taille de current_user (pour sécurité)
   - buffer        : la ligne brute reçue (ex: "LOGIN Ines") pour extraire les args
*/

void cmd_LOGIN     (int sock, char *current_user, size_t user_cap, const char *buffer);
void cmd_LIST      (int sock);
void cmd_CHALLENGE (int sock, const char *current_user, const char *buffer);
void cmd_ACCEPT    (int sock, const char *current_user, const char *buffer);
void cmd_REFUSE    (int sock, const char *current_user, const char *buffer);
void cmd_MOVE      (int sock, const char *current_user, const char *buffer);
void cmd_GAMES     (int sock);
void cmd_OBSERVE   (int sock, const char *current_user, const char *buffer);
void cmd_UNOBSERVE (int sock);
void cmd_MSG       (int sock, const char *current_user, const char *buffer);
void cmd_BROADCAST (int sock, const char *current_user, const char *buffer);
void cmd_SETBIO    (int sock, const char *current_user, const char *buffer);
void cmd_SHOWBIO   (int sock, const char *buffer);
void cmd_ADDFRIEND (int sock, const char *current_user, const char *buffer);
void cmd_PRIVATEON (int sock, const char *current_user);
void cmd_PRIVATEOFF(int sock, const char *current_user);
void cmd_HISTORY   (int sock);

#endif
