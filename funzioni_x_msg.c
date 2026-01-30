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
    int sent = 0;
    while(sent < sizeof(msg_len_network)) {
        int ret = send(sd, ((char*)&msg_len_network) + sent, sizeof(msg_len_network) - sent, 0);
        if(ret <= 0) return -1;
        sent += ret;
    }
    
    /* Invio il messaggio */
    sent = 0;
    while(sent < msg_len) {
        int ret = send(sd, msg + sent, msg_len - sent, 0);
        if(ret <= 0) return -1;
        sent += ret;
    }
    
    return 0;
}

/* Ricevi messaggio con lunghezza prefissa */
int recv_message(int sd, char *buffer, int max_len) {
    unsigned short msg_len_network;
    
    /* Ricevo la lunghezza del messaggio */
    int received = 0;
    while(received < sizeof(msg_len_network)) {
        int ret = recv(sd, ((char*)&msg_len_network) + received, sizeof(msg_len_network) - received, 0);
        if(ret <= 0) return -1;
        received += ret;
    }
    
    unsigned short msg_len = ntohs(msg_len_network);
    
    if(msg_len >= max_len) {
        printf("Errore: messaggio troppo grande\n");
        return -1;
    }
    
    /* Ricevo il messaggio */
    received = 0;
    while(received < msg_len) {
        int ret = recv(sd, buffer + received, msg_len - received, 0);
        if(ret <= 0) return -1;
        received += ret;
    }
    
    buffer[received] = '\0';
    return received;
}