// users.h
#ifndef USERS_H
#define USERS_H

int username_exists(const char *u);

int is_valid_username(const char *u);

int add_user(const char *u);

void init_users(int initial_capacity);

void free_users(void);

int get_user_count(void);

void get_user_list(char *buffer, int bufsize);

int set_user_socket(const char *u, int sock);

int get_user_socket(const char *u);

int remove_user(const char *u);

void set_socket_disconnected(int sock);

const char* get_username_by_socket(int sock);

#endif
