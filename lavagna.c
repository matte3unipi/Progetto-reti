#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "strutture_lavagna.h"



void* gestione_utente(void* arg){
    int sd_utente = (int)(intptr_t)arg;
    char buffer[256];
    int bytes_letti;

    /*Arrivo comandi da parte dell'utente*/
    while(1){
        bytes_letti = recv(sd_utente, buffer, sizeof(buffer)-1, 0);
        if (bytes_letti <= 0) {
            printf("Connessione chiusa dall'utente.\n");
            close(sd_utente);
            return NULL;
        }

        buffer[bytes_letti] = '\0';
        printf("Comando ricevuto dall'utente: %s\n", buffer);

        /*Gestione comandi*/
        if (strncmp(buffer, "HELLO:", 6) == 0) {
            int porta_utente = atoi(&buffer[6]);
            printf("Utente alla porta: %d registrato.\n", porta_utente);
            const char *risposta = "Registrazione avvenuta con successo.";
            send(sd_utente, risposta, strlen(risposta), 0);
        }
    }

    return NULL;
}


int main(){
    int ret, sd, new_socket;
    struct sockaddr_in lavagna_addr, client_addr;
    socklen_t len = sizeof(client_addr);

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

        pthread_t td;
        pthread_create(&td, NULL, gestione_utente, (void *)(intptr_t)new_socket);
        pthread_detach(td);
    }

    close(sd);
    return 0;
}