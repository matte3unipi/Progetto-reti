#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "strutture_lavagna.h"

int main(){
    int ret, sd, new_socket;
    struct sockaddr_in lavagna_addr, client_addr;
    socklen_t len = sizeof(client_addr);

    /*buffer per test*/
    char buffer[256];

    /*Creazione socket*/
    sd = socket(AF_INET, SOCK_STREAM, 0);

    if (sd < 0) {
        perror("Errore nella creazione della socket");
        exit(EXIT_FAILURE);
    }

    /* Creazione indirizzo lavagna*/
    memset(&lavagna_addr, 0, sizeof(lavagna_addr));
    lavagna_addr.sin_family = AF_INET;
    lavagna_addr.sin_port = htons(PORTA_LAVAGNA);
    inet_pton(AF_INET, "127.0.0.1", &lavagna_addr.sin_addr);

    /* Binding della socket */
    ret = bind(sd, (struct sockaddr *)&lavagna_addr, sizeof(lavagna_addr));
    ret = listen(sd, MIN_UTENTI);

    /* Accettazione connessioni in arrivo */
    while (1)
    {
        printf("In attesa di connessioni...\n");

        if((new_socket = accept(sd, (struct sockaddr *)&client_addr, &len)) < 0){
            perror("Errore nell'accettazione della connessione");
            close(sd);
            exit(EXIT_FAILURE);
        }

        printf("Connessione accettata da un utente.\n");

        /* Gestione comunicazione con l'utente */
        while (1) {
            int bytes_read = recv(new_socket, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) {
                printf("Utente disconnesso.\n");
                close(new_socket);
                break;
            }
            printf("Messaggio ricevuto dall'utente: %s\n", buffer);

            /* Echo del messaggio ricevuto */
            send(new_socket, buffer, bytes_read, 0);
        }
    }

    close(sd);
    return 0;
}