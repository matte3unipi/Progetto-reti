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
#include "funzioni_x_msg.h"


int registrazione_utente(int porta_utente){
    /* Controllo se la porta è già registrata */
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(lavagna->porta_utenti_connessi[i] == porta_utente){
            return -1;
        }
    }

    /* Aggiungo in coda l'utente*/
    lavagna->numero_utenti_connessi++;
    lavagna->porta_utenti_connessi = realloc(lavagna->porta_utenti_connessi, 
        lavagna->numero_utenti_connessi * sizeof(int));

    if(lavagna->porta_utenti_connessi == NULL){
        perror("Errore nella registrazione dell'utente");
        exit(EXIT_FAILURE);
    }

    /*Salvo la porta dell'utente in coda*/
    lavagna->porta_utenti_connessi[lavagna->numero_utenti_connessi - 1] = porta_utente;

    return 0;
}

int rimozione_utente(int porta_utente){
    int trovato = 0;

    /*Cerco l'utente e lo rimuovo dalla lista*/
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(lavagna->porta_utenti_connessi[i] == porta_utente) {
            trovato = 1;
        }
        if(trovato && i < lavagna->numero_utenti_connessi - 1) {
            lavagna->porta_utenti_connessi[i] = lavagna->porta_utenti_connessi[i + 1];
        }
    }

    /*Se ho trovato l'utente rialloco le porte*/
    if(trovato){
        lavagna->numero_utenti_connessi--;
        lavagna->porta_utenti_connessi = realloc(lavagna->porta_utenti_connessi, 
            lavagna->numero_utenti_connessi * sizeof(int));

        if(lavagna->numero_utenti_connessi > 0 && lavagna->porta_utenti_connessi == NULL){
            perror("Errore nella rimozione dell'utente");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

int show_lavagna(){
    // Funzione per mostrare lo stato della lavagna (placeholder)
    return 0;
}


void* gestione_utente(void* arg){
    int sd_utente = (int)(intptr_t)arg;
    char buffer[256];
    int porta_utente = 0;

    /*Arrivo comandi da parte dell'utente*/
    while(1){
        /*Ricezione messaggio*/
        if(recv_message(sd_utente, buffer, sizeof(buffer)) < 0) {
            printf("Connessione utente sulla porta %d chiusa.\n", sd_utente);
            close(sd_utente);
            return NULL;
        }

        printf("Comando ricevuto dall'utente: %s\n", buffer);

        /*Gestione comandi*/
        /*Comando HELLO*/
        if (strncmp(buffer, "HELLO:", 6) == 0) {
            porta_utente = atoi(&buffer[6]);

            if (registrazione_utente(porta_utente) < 0) {
                const char *risposta = "Porta già registrata.";
                send_message(sd_utente, risposta);
                continue;
            }
            
            printf("Utente alla porta %d registrato.\n", porta_utente);
            const char *risposta = "Registrazione avvenuta con successo.";
            send_message(sd_utente, risposta);
            continue;   
        }
        
        /*Comando SHOW_LAVAGNA*/
        if (strcmp(buffer, "SHOW_LAVAGNA") == 0) {
            show_lavagna();
            const char *risposta = "Stato lavagna mostrato.";
            send_message(sd_utente, risposta);
            continue;
        }

        /*Comando QUIT*/
        if (strncmp(buffer, "QUIT:", 5) == 0) {
            printf("Utente sulla porta %d disconnesso.\n", porta_utente);
            rimozione_utente(porta_utente);
            const char *risposta = "Connessione terminata.";
            send_message(sd_utente, risposta);
            close(sd_utente);
            break;
        }

    }

    return NULL;
}

/*Funzione per la creazione della lavagna e inizializzazione*/
void creazione_lavagna(){
    lavagna = malloc(sizeof(struct st_LAVAGNA));

    if(lavagna == NULL){
        perror("Errore nell'allocazione della lavagna");
        exit(EXIT_FAILURE);
    }

    lavagna->id = 1;
    lavagna->numero_utenti_connessi = 0;
    lavagna->numero_card_totali = 0;
    lavagna->porta_utenti_connessi = NULL;

    for(int i = 0; i < NUM_COLONNE; i++){
        lavagna->colonne[i].stato = (STATO_COLONNA)i;
        lavagna->colonne[i].numero_card = 0;
        lavagna->colonne[i].cards = NULL;
    }

    printf("Lavagna creata e pronta a ricevere connessioni.\n");
}


int main(){
    creazione_lavagna();

    int sd, new_socket;
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
    bind(sd, (struct sockaddr *)&lavagna_addr, sizeof(lavagna_addr));
    listen(sd, MIN_UTENTI);

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