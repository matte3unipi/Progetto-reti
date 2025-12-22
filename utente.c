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
int hello_eseguito = 0;
int porta_utente = 0;
pthread_mutex_t ascolto;

struct CARD_ASSEGNATA {
    int id;
    char testo[256];
    int porte_utenti[100];
    int num_utenti;
    int review_ricevute;
    int card_done_inviata;
};
struct CARD_ASSEGNATA card_assegnata;



/*Funzione per gestire il comando HELLO */
void hello_function(int sd) {
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
int quit_function(int sd) {
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
void create_card_function(int sd) {
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
void request_user_list_function(int sd) {
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


/*Funzione per salvare la lista delle porte utenti ricevuta*/
void save_user_list(const char* msg) {
    printf("Ricevuta lista utenti dalla lavagna.\n");

    char lista_copia[1024];
    strncpy(lista_copia, msg, sizeof(lista_copia));
    lista_copia[sizeof(lista_copia) - 1] = '\0';    
    char *token = strtok(lista_copia, ":");
    card_assegnata.num_utenti = atoi(token);
    card_assegnata.review_ricevute = 0;
    for(int i = 0; i < card_assegnata.num_utenti; i++) {
        token = strtok(NULL, ":");
        card_assegnata.porte_utenti[i] = atoi(token);
    }
    printf("Lista utenti salvata\n");
    return;
}

/*Funzione per gestire l'arrivo di una card*/
void handle_card_function(const char* msg) {
    /* Copio il messaggio per lavorare*/
    char testo_card[1024];
    strncpy(testo_card, msg, sizeof(testo_card) - 1);
    testo_card[sizeof(testo_card) - 1] = '\0';

    char* token = strtok(testo_card, ":");
    card_assegnata.id = atoi(token);
    token = strtok(NULL, ":");
    strncpy(card_assegnata.testo, token, sizeof(card_assegnata.testo) - 1);
    card_assegnata.testo[sizeof(card_assegnata.testo) - 1] = '\0';
    token = strtok(NULL, ":");
    card_assegnata.num_utenti = atoi(token);
    card_assegnata.review_ricevute = 0;
    card_assegnata.card_done_inviata = 0;

    for(int i = 0; i < card_assegnata.num_utenti; i++) {
        token = strtok(NULL, ":");
        card_assegnata.porte_utenti[i] = atoi(token);
    }
    printf("\nNuova card assegnata:\n");
    printf("ID: %d\n", card_assegnata.id);
    printf("Testo: %s\n", card_assegnata.testo);
    return;
}

/*Funzione per gestire il comando ACK_CARD */
void ack_card_function(int sd) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return;
    }

    char msg[50];
    sprintf(msg, "ACK_CARD:%d", card_assegnata.id);
    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio ACK_CARD");
        exit(EXIT_FAILURE);
    } else {
        printf("Comando ACK_CARD eseguito per la card ID %d.\n", card_assegnata.id);
    }
}

/*Funzione per gestire il comando CARD_DONE */
void card_done_function(int sd) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return;
    }

    if(card_assegnata.card_done_inviata == 1) {
        printf("Hai già inviato il comando CARD_DONE per questa card.\n");
        return;
    }

    if(card_assegnata.num_utenti != card_assegnata.review_ricevute){
        printf("Non puoi completare la card, non tutte le review sono state ricevute.\n");
        return;
    }

    char msg[50];
    sprintf(msg, "CARD_DONE:%d", card_assegnata.id);
    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio CARD_DONE");
        exit(EXIT_FAILURE);
    } else {
        printf("Comando CARD_DONE eseguito per la card ID %d.\n", card_assegnata.id);
        card_assegnata.card_done_inviata = 1;
    }

    return;
}


/*Thread per socket p2p*/
void* gestione_p2p_review(void* arg) {
    int porta_destinatario = (int)(intptr_t)arg;

    /* Creazione socket */
    int sd_p2p;
    struct sockaddr_in addr_destinatario;

    sd_p2p = socket(AF_INET, SOCK_STREAM, 0);
    if (sd_p2p < 0) {
        perror("Errore nella creazione della socket P2P");
        return NULL;
    }

    /* Creazione indirizzo pear destinatario */
    memset(&addr_destinatario, 0, sizeof(addr_destinatario));
    addr_destinatario.sin_family = AF_INET;
    addr_destinatario.sin_port = htons(porta_destinatario);
    if(inet_pton(AF_INET, "127.0.0.1", &addr_destinatario.sin_addr) <= 0) {
        perror("Errore nella conversione dell'indirizzo IP P2P");
        close(sd_p2p);
        return NULL;
    }
    /* Connessione al destinatario */
    if(connect(sd_p2p, (struct sockaddr *)&addr_destinatario, sizeof(addr_destinatario)) < 0) {
        perror("Errore nella connessione P2P");
        close(sd_p2p);
        return NULL;
    }
    /* Invio della review */
    char review_msg[64];
    sprintf(review_msg, "REVIEW_CARD:%d", card_assegnata.id);
    if(send_message(sd_p2p, review_msg) < 0) {
        perror("Errore nell'invio della REVIEW_CARD");
        close(sd_p2p);
        return NULL;
    }
    printf("REVIEW_CARD inviata alla porta %d.\n", porta_destinatario);

    /* Attendo la risposta */
    char ack[20];
    if(recv_message(sd_p2p, ack, sizeof(ack) - 1) <= 0) {
        perror("Errore nella ricezione dell'ACK della REVIEW_CARD");
        close(sd_p2p);
        return NULL;
    }
    if(strncmp(ack, "ACK_REVIEW_CARD", 15) == 0) {
        pthread_mutex_lock(&ascolto);
        card_assegnata.review_ricevute++;
        pthread_mutex_unlock(&ascolto);
        printf("ACK_REVIEW_CARD ricevuto dalla porta %d.\n", porta_destinatario);
    }
    close(sd_p2p);
    return NULL;
}

/*Funzione per gestire il comando REVIEW_CARD */
void review_card_function(int sd) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return;
    }

    card_assegnata.review_ricevute = 0;

    for(int i = 0; i < card_assegnata.num_utenti; i++) {
        int porta_destinatario = card_assegnata.porte_utenti[i];

        pthread_t p2p_socket;
        pthread_create(&p2p_socket, NULL, gestione_p2p_review, (void *)(intptr_t)porta_destinatario);
        pthread_detach(p2p_socket);
    }

    sleep(4);

    if(card_assegnata.review_ricevute == (card_assegnata.num_utenti - 1)) {
        printf("REVIEW_CARD ricevute.\n");
    } else {
        printf("Non tutte le review sono state ricevute.\n");
    }
}

/* Thread server P2P per ricevere REVIEW_CARD */
void* server_p2p(void* arg) {
    int porta_utente = (int)(intptr_t)arg;

    int sd_server, sd_client;
    struct sockaddr_in addr_server, addr_client;
    socklen_t len_client = sizeof(addr_client);

    /* Creazione socket server */
    sd_server = socket(AF_INET, SOCK_STREAM, 0);
    if(sd_server < 0) {
        perror("Errore nella creazione socket server P2P");
        return NULL;
    }

    /* Creazione indirizzo server P2P */
    memset(&addr_server, 0, sizeof(addr_server));
    addr_server.sin_family = AF_INET;
    addr_server.sin_port = htons(porta_utente);
    if(inet_pton(AF_INET, "127.0.0.1", &addr_server.sin_addr) <= 0) {
        perror("Errore nella conversione dell'indirizzo IP server P2P");
        close(sd_server);
        return NULL;
    }

    /* Binding del socket server P2P */
    if(bind(sd_server, (struct sockaddr *)&addr_server, sizeof(addr_server)) < 0) {
        perror("Errore nel bind server P2P");
        close(sd_server);
        return NULL;
    }

    listen(sd_server, 5);

    char buffer[64];

    while(1) {
        /* Accetto connessione in arrivo */
        if((sd_client = accept(sd_server, (struct sockaddr *)&addr_client, &len_client)) < 0) {
            perror("Errore nell'accept P2P");
            close(sd_server);
            return NULL;
        }

        /* Ricevi REVIEW_CARD */
        if(recv_message(sd_client, buffer, sizeof(buffer)) > 0) {
            if(strncmp(buffer, "REVIEW_CARD:", 12) == 0) {
                
                /* Invio ACK_REVIEW_CARD */
                char ack_msg[] = "ACK_REVIEW_CARD";
                if(send_message(sd_client, ack_msg) < 0) {
                    perror("Errore nell'invio dell'ACK_REVIEW_CARD");
                }
                close(sd_client);
            }
        }
    }
    close(sd_server);
    return NULL;
}


/*Thread per gestire i messaggi in arrivo dalla lavagna*/
void* gestione_ascolto(void* arg) {
    int sd = (int)(intptr_t)arg;
    char buffer[1024];
    
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
            save_user_list(buffer + 15);
        }
        /* Nuova card assegnata */
        else if(strncmp(buffer,"HANDLE_CARD:",12)==0){
            handle_card_function(buffer + 12);
        }
        /* Risposta al ping */
        else if(strncmp(buffer, "PING", 4) == 0){
            char pong_msg[] = "PONG";
            if(send_message(sd, pong_msg) < 0){
                perror("Errore nell'invio del PONG");
            }
        }

        /* Messaggi generici */
        printf("\n[LAVAGNA] %s\n", buffer);

    }
    
    return NULL;
}



/*--------------------MAIN-------------------*/
int main(int argc, char *argv[]) {
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

    /* Server P2P per ricevere REVIEW_CARD */
    pthread_t thread_server_p2p;
    pthread_create(&thread_server_p2p, NULL, server_p2p, (void *)(intptr_t)porta_utente);
    pthread_detach(thread_server_p2p);
    
    /*Creazione thread per ascoltare dati in arrivo */
    pthread_t thread_ascolto;
    pthread_create(&thread_ascolto, NULL, gestione_ascolto, (void *)(intptr_t)sd);
    pthread_detach(thread_ascolto);

    /* Controllo che i comandi inseriti da tastiera siano quelli permessi.*/
    char comando[20];

    while (1){        
        printf("Inserisci il comando\n");
        scanf("%s", comando);

       printf(RIGA_SEPARATORIA);

       // Invia il comando alla lavagna
       if(strcmp(comando, "HELLO") == 0) {
           hello_function(sd);
       }
       else if(strcmp(comando,"QUIT")== 0) {
           if (quit_function(sd) == 0) {
               close(sd);
               return EXIT_SUCCESS;
           }
       }
       else if(strcmp(comando, "CREATE_CARD") == 0) {
           create_card_function(sd);
       }
       else if(strcmp(comando, "REQUEST_USER_LIST") == 0) {
           request_user_list_function(sd);
       }
       else if(strcmp(comando, "ACK_CARD") == 0) {
           ack_card_function(sd);
       }
       else if(strcmp(comando,"REVIEW_CARD") == 0){
            review_card_function(sd);
       }
       else if(strcmp(comando, "CARD_DONE") == 0) {
            card_done_function(sd);
       }
       else {
           printf("Comando non riconosciuto.\n");
       }

       sleep(1);
       printf(RIGA_SEPARATORIA);
    }

    close(sd);
    return EXIT_SUCCESS;
}