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
#include "funzioni_x_msg.h"

/*Definizioni*/
#define PORTA_LAVAGNA 5678
#define RIGA_SEPARATORIA "<----------------------------------------\n"

/*Dati utili per il codice*/
const char *comandi_validi[] = {
    "HELLO", "QUIT", "CREATE_CARD", 
    "REQUEST_USER_LIST", "REVIEW_CARD", "CARD_DONE",
    "SHOW_LAVAGNA"
};
const int num_comandi = 7;
int hello_eseguito = 0;
int lista_utenti [100];
pthread_mutex_t ascolto = PTHREAD_MUTEX_INITIALIZER;



/*Funzione per gestire il comando HELLO */
void hello_function(int sd, int porta_utente) {
    if(hello_eseguito) {
        printf("Comando HELLO già eseguito in questa sessione.\n");
        return;
    }

    char msg[20];
    sprintf(msg, "HELLO:%d", porta_utente);

    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio HELLO");
        exit(EXIT_FAILURE);
    } else {
        printf("Registrazione in corso...\n");
    }

    return;
}

/*Funzione per gestire il comando QUIT */
int quit_function(int sd, int porta_utente) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return -1;
    }

    char msg[20];
    sprintf(msg, "QUIT:%d", porta_utente);

    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio QUIT");
        exit(EXIT_FAILURE);
    } else {
        printf("Comando QUIT eseguito.\n");
    }

    hello_eseguito = 0;

    return 0;
}


/*Funzione per gestire il comando CREATE_CARD */
void create_card_function(int sd, int porta_utente) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return;
    }

    printf(" > Creazione nuova card < \n");

    char id_card_str[8];
    printf("Inserisci l'ID della nuova card: ");
    scanf("%s", id_card_str);

    char colonna_card_str[16];
    printf("Inserisci la colonna della nuova card (TO_DO, DOING, DONE): ");
    scanf("%s", colonna_card_str);

    char testo_card_str[256];
    printf("Inserisci il testo attività della nuova card: ");
    scanf(" %[^\n]", testo_card_str);

    char msg[300];
    sprintf(msg, "CREATE_CARD:%s:%s:%s", id_card_str, colonna_card_str, testo_card_str);

    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio CREATE_CARD");
        exit(EXIT_FAILURE);
    } else {
        printf("Comando CREATE_CARD eseguito.\n");
    }

    return;
}

/*Funzione per gestire il comando REQUEST_USER_LIST */
void request_user_list_function(int sd, int porta_utente) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return;
    }

    char msg[] = "REQUEST_USER_LIST";
    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio REQUEST_USER_LIST");
        exit(EXIT_FAILURE);
    } else {
        printf("Comando REQUEST_USER_LIST eseguito.\n");
    }

    return;
}


/*Funzione per gestire i messaggi in arrivo dalla lavagna*/
void* gestione_ascolto(void* arg) {
    int sd = (int)(intptr_t)arg;
    char buffer[256];
    
    while(1) {
        if(recv_message(sd, buffer, sizeof(buffer) - 1) <= 0) {
            printf("Connessione chiusa dalla lavagna.\n");
            break;
        }
    
        /* Registrazione HELLO */
        if (strncmp(buffer, "Registrazione avvenuta con successo.", 36) == 0) {
            hello_eseguito = 1;
        }
        /* Lista utenti ricevuta */
        else if (strncmp(buffer, "SEND_USER_LIST:", 15) == 0) {
            /* Salvo la lista per comandi futuri */
            char copia[256];
            strncpy(copia, buffer, sizeof(copia) - 1);
            char *token = strtok(copia, ":");
            strtok(NULL, ":"); 
            int num_utenti = 0;
            while((token = strtok(NULL, ":")) != NULL) {
                lista_utenti[num_utenti++] = atoi(token);
            }
        }
        else if(strncmp(buffer,"HANDLER_CARD:",13)==0){
            handler_card_function(buffer + 13);
        }
        
        /* Messaggi generici */
        printf("\n[LAVAGNA] %s\n", buffer);

    }
    
    return NULL;
}



/*--------------------MAIN-------------------*/
int main(int argc, char *argv[]) {
    int porta_utente = 0;
    pthread_mutex_init(&ascolto, NULL);
    
    if (argc != 2) {
        printf("L'eseguibile richiedela porta.\n");
        return -1;
    }
    
    porta_utente = atoi(argv[1]);

    /*Creazione socket*/
    int sd, ret;
    struct sockaddr_in lavagna_addr;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) {
        perror("Errore nella creazione della socket");
        exit(EXIT_FAILURE);
    }

    /* Creazione indirizzo utente*/
    memset(&lavagna_addr, 0, sizeof(lavagna_addr));
    lavagna_addr.sin_family = AF_INET;
    lavagna_addr.sin_port = htons(PORTA_LAVAGNA);
    if(inet_pton(AF_INET, "127.0.0.1", &lavagna_addr.sin_addr) <= 0) {
        perror("Errore nella conversione dell'indirizzo IP");
        close(sd);
        exit(EXIT_FAILURE);
    }

    /*Connessione alla lavagna*/
    if((ret = connect(sd, (struct sockaddr *)&lavagna_addr, sizeof(lavagna_addr))) < 0) {
        perror("Errore nella connessione alla lavagna");
        close(sd);
        exit(EXIT_FAILURE);
    }

    printf("Connessione alla lavagna avvenuta con successo.\n");
    
    /*Creazione thread per ascoltare dati in arrivo */
    pthread_t thread_ascolto;
    pthread_create(&thread_ascolto, NULL, gestione_ascolto, (void *)(intptr_t)sd);
    pthread_detach(thread_ascolto);

    /* Controllo che i comandi inseriti da tastiera siano quelli permessi.*/
    char comando[20];

    while (1){        
        printf("Inserisci il comando\n");
        scanf("%s", comando);

        int comando_valido = 0;
        for (int i = 0; i < num_comandi; i++) {
            if (strcmp(comando, comandi_validi[i]) == 0) {
                comando_valido = 1;

                printf(RIGA_SEPARATORIA);

                // Invia il comando alla lavagna
                if(strcmp(comando, "HELLO") == 0) {
                    hello_function(sd, porta_utente);
                }
                if(strcmp(comando,"QUIT")== 0) {
                    if (quit_function(sd, porta_utente) == 0) {
                        close(sd);
                        return EXIT_SUCCESS;
                    }
                }
                if(strcmp(comando, "CREATE_CARD") == 0) {
                    create_card_function(sd, porta_utente);
                }
                if(strcmp(comando, "REQUEST_USER_LIST") == 0) {
                    request_user_list_function(sd, porta_utente);
                }

                sleep(1);
                printf(RIGA_SEPARATORIA);
                break;
            }
        }
        if (!comando_valido) {
            printf("Comando non permesso.\n");
            continue;
        }
    }
    
    close(sd);
    return EXIT_SUCCESS;
}