#ifndef FUNZIONI_X_MSG_H
#define FUNZIONI_X_MSG_H

int send_message(int sd, const char *msg);
int recv_message(int sd, char *buffer, int max_len);

#endif