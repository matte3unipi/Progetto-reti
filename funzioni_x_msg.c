#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

/* Invia messaggio con lunghezza prefissa */
int send_message(int sd, const char *msg) {
    unsigned short msg_len = strlen(msg);
    unsigned short msg_len_network = htons(msg_len);
    
    /* Invio la lunghezza del messaggio */
    if (send(sd, &msg_len_network, sizeof(msg_len_network), 0) < 0) {
        return -1;
    }
    
    /* Invio il messaggio */
    if (send(sd, msg, msg_len, 0) < 0) {
        return -1;
    }
    
    return 0;
}

/* Ricevi messaggio con lunghezza prefissa */
int recv_message(int sd, char *buffer, int max_len) {
    unsigned short msg_len_network;
    
    /* Ricevo la lunghezza del messaggio */
    if (recv(sd, &msg_len_network, sizeof(msg_len_network), 0) <= 0) {
        return -1;
    }
    
    unsigned short msg_len = ntohs(msg_len_network);
    
    if (msg_len >= max_len) {
        printf("Errore: messaggio troppo grande\n");
        return -1;
    }
    
    /* Ricevo il messaggio */
    int bytes_letti = recv(sd, buffer, msg_len, 0);
    if (bytes_letti != msg_len) {
        return -1;
    }
    buffer[bytes_letti] = '\0';
    return bytes_letti;
}