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
#include <errno.h>
#include "funzioni_x_msg.h"
#include "strutture_utente.h"


/*
* ============================================================================ *
* ============================================================================ *
                    Funzioni gestione comandi utente
* ============================================================================ *
* ============================================================================ *
*/

/*
Funzione per gestire il comando HELLO 
    @param sd: socket
*/
void hello_function(int sd) {
    if(hello_eseguito) {
        printf("Comando HELLO già eseguito in questa sessione.\n");
        return;
    }

    char msg[20];
    sprintf(msg, "HELLO%s%d", CARATTERE_SEPARATORE, porta_utente);

    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio HELLO");
        exit(EXIT_FAILURE);
    } else {
        printf("Registrazione in corso...\n");
    }

    return;
}

/*
Funzione per gestire il comando QUIT 
    @param sd: socket
*/
int quit_function(int sd) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return -1;
    }

    char msg[20];
    sprintf(msg, "QUIT");

    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio QUIT");
        exit(EXIT_FAILURE);
    } else {
        printf("Comando QUIT eseguito.\n");
    }

    hello_eseguito = 0;

    return 0;
}


/*
Funzione per gestire il comando SHOW_LAVAGNA 
    @param sd: socket
*/
void show_lavagna_function(int sd) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return;
    }

    char msg[] = "SHOW_LAVAGNA";
    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio SHOW_LAVAGNA");
        exit(EXIT_FAILURE);
    }

    return;
}


/*
Funzione per gestire il comando CREATE_CARD 
    @param sd: socket
*/
void create_card_function(int sd) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return;
    }

    printf(" > Creazione nuova card < \n");

    /* Per ogni campo verifico che i caratteri inseriti non superino 
    il limite massimo consentito.*/

    char id_card_str[8];
    printf("Inserisci l'ID della nuova card: ");
    if(fgets(id_card_str, sizeof(id_card_str), stdin) == NULL) {
        perror("Errore nella lettura dell'ID della card");
        return;
    }
    if(strchr(id_card_str, '\n') == NULL) {
        printf("Errore: l'ID è troppo lungo (max 7 caratteri).\n");
        int c;
        while((c = getchar()) != '\n' && c != EOF);
        return;
    }
    id_card_str[strcspn(id_card_str, "\n")] = 0;

    char colonna_card_str[8];
    printf("Inserisci la colonna della nuova card (TO_DO): ");
    if(fgets(colonna_card_str, sizeof(colonna_card_str), stdin) == NULL) {
        perror("Errore nella lettura della colonna della card");
        return;
    }
    if(strchr(colonna_card_str, '\n') == NULL) {
        printf("Errore: la colonna è troppo lunga (max 7 caratteri).\n");
        int c;
        while((c = getchar()) != '\n' && c != EOF);
        return;
    }
    colonna_card_str[strcspn(colonna_card_str, "\n")] = 0;

    char testo_card_str[256];
    printf("Inserisci il testo attività della nuova card: ");
    if(fgets(testo_card_str, sizeof(testo_card_str), stdin) == NULL) {
        perror("Errore nella lettura del testo della card");
        return;
    }
    if(strchr(testo_card_str, '\n') == NULL) {
        printf("Errore: il testo è troppo lungo (max 255 caratteri).\n");
        int c;
        while((c = getchar()) != '\n' && c != EOF);
        return;
    }
    /* Controllo l'inserimento del carattere separatore*/
    if(strstr(testo_card_str, CARATTERE_SEPARATORE) != NULL) {
        printf("Errore: il carattere '%s' non è consentito nel testo.\n", CARATTERE_SEPARATORE);
        return;
    }
    testo_card_str[strcspn(testo_card_str, "\n")] = 0;

    char msg[300];
    snprintf(msg, sizeof(msg), "CREATE_CARD%s%s%s%s%s%s", CARATTERE_SEPARATORE, id_card_str, 
    CARATTERE_SEPARATORE, colonna_card_str, CARATTERE_SEPARATORE, testo_card_str);

    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio CREATE_CARD");
        exit(EXIT_FAILURE);
    } else {
        printf("Comando CREATE_CARD eseguito.\n");
    }

    return;
}

/*
Funzione per gestire il comando REQUEST_USER_LIST 
    @param sd: socket    
*/
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

/*
Funzione per gestire il comando ACK_CARD 
    @param sd: socket
*/
void ack_card_function(int sd) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return;
    }

    if(card_assegnata.ack_card_inviata == 1) {
        printf("Hai già inviato il comando ACK_CARD per questa card.\n");
        return;
    }

    if(card_assegnata.id < 0) {
        printf("Non hai nessuna card assegnata.\n");
        return;
    }

    char msg[50];
    sprintf(msg, "ACK_CARD%s%d", CARATTERE_SEPARATORE, card_assegnata.id);
    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio ACK_CARD");
        exit(EXIT_FAILURE);
    } else {
        printf("Comando ACK_CARD eseguito per la card ID %d.\n", card_assegnata.id);
        card_assegnata.ack_card_inviata = 1;
    }
}

/*
Funzione per gestire il comando CARD_DONE 
    @param sd: socket
*/
void card_done_function(int sd) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return;
    }

    if(card_assegnata.id < 0) {
        printf("Non hai nessuna card assegnata.\n");
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
    sprintf(msg, "CARD_DONE%s%d", CARATTERE_SEPARATORE, card_assegnata.id);
    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio CARD_DONE");
        exit(EXIT_FAILURE);
    } else {
        printf("Comando CARD_DONE eseguito per la card ID %d.\n", card_assegnata.id);
        card_assegnata.card_done_inviata = 1;
        card_assegnata.id = -1;  /* Reset card assegnata */
    }

    return;
}

/*
Funzione per gestire il comando PONG_LAVAGNA 
    @param sd: socket
*/
void pong_lavagna_function(int sd) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return;
    }

    if(ping_ricevuto == 0) {
        printf("Non hai ricevuto nessun PING dalla lavagna.\n");
        return;
    }

    char msg[] = "PONG_LAVAGNA";
    if(send_message(sd, msg) < 0) {
        perror("Errore durante l'invio del messaggio PONG_LAVAGNA");
        exit(EXIT_FAILURE);
    } else {
        printf("PONG inviato.\n");
        ping_ricevuto = 0;
    }

    return;
}


/*
* ============================================================================ *
* ============================================================================ *
                    Funzioni gestione messaggi da lavagna
* ============================================================================ *
* ============================================================================ *
*/


/*
Funzione per salvare la lista delle porte utenti ricevuta
    @param msg: lista utenti
*/
void save_user_list(const char* msg) {
    printf("Ricevuta lista utenti dalla lavagna.\n");

    char lista_copia[1024];
    strncpy(lista_copia, msg, sizeof(lista_copia));
    lista_copia[sizeof(lista_copia) - 1] = '\0';    
    char *token = strtok(lista_copia, CARATTERE_SEPARATORE);
    card_assegnata.num_utenti = atoi(token);
    card_assegnata.review_ricevute = 0;

    if(card_assegnata.porte_utenti != NULL) {
        free(card_assegnata.porte_utenti);
    }
    card_assegnata.porte_utenti = malloc(card_assegnata.num_utenti * sizeof(int));
    for(int i = 0; i < card_assegnata.num_utenti; i++) {
        token = strtok(NULL, CARATTERE_SEPARATORE);
        card_assegnata.porte_utenti[i] = atoi(token);
    }
    printf("Lista utenti salvata\n");
    return;
}

/*
Funzione per gestire l'arrivo di una card
    @param msg: dati card id|testo|porte utenti
*/
void handle_card_function(const char* msg) {
    /* Copio il messaggio per lavorare*/
    char testo_card[1024];
    strncpy(testo_card, msg, sizeof(testo_card) - 1);
    testo_card[sizeof(testo_card) - 1] = '\0';

    /* Estraggo i dati della card */
    char* token = strtok(testo_card, CARATTERE_SEPARATORE);
    card_assegnata.id = atoi(token);
    token = strtok(NULL, CARATTERE_SEPARATORE);
    strncpy(card_assegnata.testo, token, sizeof(card_assegnata.testo) - 1);
    card_assegnata.testo[sizeof(card_assegnata.testo) - 1] = '\0';
    token = strtok(NULL, CARATTERE_SEPARATORE);
    card_assegnata.num_utenti = atoi(token);
    card_assegnata.review_ricevute = 0;
    card_assegnata.card_done_inviata = 0;
    card_assegnata.ack_card_inviata = 0;

    /* Verifico se ho in memoria già allocato porte utenti (potrebbero essere non piu utilizzate alcune)*/
    if(card_assegnata.porte_utenti != NULL) {
        free(card_assegnata.porte_utenti);
    }
    card_assegnata.porte_utenti = malloc(card_assegnata.num_utenti * sizeof(int));
    if(card_assegnata.porte_utenti == NULL) {
        perror("Errore nell'allocazione delle porte");
        return;
    }

    for(int i = 0; i < card_assegnata.num_utenti; i++) {
        token = strtok(NULL, CARATTERE_SEPARATORE);
        card_assegnata.porte_utenti[i] = atoi(token);
    }

    /* Stampo a schermo i dati essenziali della card */
    printf("\n>>> Nuova card assegnata:\n");
    printf("     # ID: %d\n", card_assegnata.id);
    printf("     # Testo: %s\n\n", card_assegnata.testo);
    return;
}

void print_lavagna(const char* msg) {
    char msg_copia[8192];
    strncpy(msg_copia, msg, sizeof(msg_copia) - 1);
    msg_copia[sizeof(msg_copia) - 1] = '\0';
    
    /* Formato: LAVAGNA_STATE|id_lavagna|max_cards||card1_col1|card2_col1|...||card1_col2|card2_col2...|| */
    char *ptr = msg_copia;

    /* Estraggo l'id della lavagna */
    char* s_id = strstr(ptr, CARATTERE_SEPARATORE);
    if(!s_id) 
        return;
    *s_id = '\0';
    int id_lavagna = atoi(ptr);
    ptr = s_id + strlen(CARATTERE_SEPARATORE);
    
    /* Estraggo max_cards */
    char* s_max = strstr(ptr, CARATTERE_SEPARATORE);
    if(!s_max) 
        return;
    *s_max = '\0';
    int max_cards = atoi(ptr);
    ptr = s_max + strlen(CARATTERE_SEPARATORE) + strlen(CARATTERE_SEPARATORE); // Salto il secondo separatore
    
    /* Array per memorizzare le card di ogni colonna */
    struct {
        int id;
        char testo[256];
    } cards[3][50];
    int card_count[3] = {0};
    

    /* Parse card da ogni colonna */
    for(int col_idx = 0; col_idx < 3; col_idx++) {
        /* Trovo la posizione del prossimo || che separa le colonne */
        char *col_end = strstr(ptr, CARATTERE_SEPARATORE CARATTERE_SEPARATORE);
        if(!col_end) 
            col_end = ptr + strlen(ptr);  /* Fine stringa */
        
        /* Se col_end != ptr, c'è almeno una card */
        if(col_end != ptr) {
            char *card_start = ptr;

             /* Parsing manuale -> leggo coppie id|testo fino a || */
            while(card_start < col_end) {
                /* Salta eventuali | iniziali */
                while(*card_start == '|' && card_start < col_end) card_start++;

                /* Controllo fine colonna */
                if(card_start >= col_end) break;

                /* ID */
                char *sep1 = strchr(card_start, '|');
                if(!sep1 || sep1 >= col_end) break;

                *sep1 = '\0';
                int card_id = atoi(card_start);
                char *testo_start = sep1 + 1;

                /* Testo */
                char *sep2 = strchr(testo_start, '|');
                char testo[256];

                if(sep2 && sep2 < col_end) {
                    /* C'è un'altra card dopo */
                    int len = sep2 - testo_start;
                    strncpy(testo, testo_start, len);
                    testo[len] = '\0';
                    card_start = sep2 + 1;
                } else {
                    /* Ultima card della colonna */
                    int len = col_end - testo_start;
                    strncpy(testo, testo_start, len);
                    testo[len] = '\0';
                    card_start = col_end;
                }

                /* Salva card */
                cards[col_idx][card_count[col_idx]].id = card_id;
                strncpy(cards[col_idx][card_count[col_idx]].testo, testo, 255);
                cards[col_idx][card_count[col_idx]].testo[255] = '\0';
                card_count[col_idx]++;
            }
        }

        ptr = col_end;
        /* Salto il separatore di colonna || */
        if(*ptr == '|' && *(ptr + 1) == '|') ptr += 2;
    }
    
    /* Stampa */
    printf("\n----------------------------------------------------------------\n");
    printf("|                         Lavagna - %d                          |\n", id_lavagna);
    printf("----------------------------------------------------------------\n");
    printf("|        TO DO       |        DOING       |        DONE        |\n");
    printf("----------------------------------------------------------------\n");
    
    for(int i = 0; i < max_cards; i++){
        /* Riga 1: ID */
        for(int j = 0; j < 3; j++){
            if(i < card_count[j]){
                printf("|        ID: %-8d", cards[j][i].id);
            } else {
                printf("|                    ");
            }
        }
        printf("|\n");
        
        /* Riga 2 e seguenti: Testo con wrapping */
        int fine = 0;
        int line = 0;
        while(fine == 0) {
            for(int j = 0; j < 3; j++){
                if(i < card_count[j]){
                    int start = line * 18;
                    if(start < strlen(cards[j][i].testo)){
                        char segmento[19];
                        strncpy(segmento, &cards[j][i].testo[start], 18);
                        segmento[18] = '\0';
                        printf("| %-18s ", segmento);
                    } else {
                        printf("|                    ");
                    }
                } else {
                    printf("|                    ");
                }
            }
            printf("|\n");
            line++;
            fine = 1;
            for(int j = 0; j < 3; j++){
                if(i < card_count[j]){
                    int start = line * 18;
                    if(start < strlen(cards[j][i].testo)){
                        fine = 0;
                        break;
                    }
                }
            }
        }
        printf("----------------------------------------------------------------\n");
    }

    printf("\n");
    return;
}


/*
* ============================================================================ *
* ============================================================================ *
                Parte gestione comunicazioni P2P per REVIEW_CARD
* ============================================================================ *
* ============================================================================ *
*/

/*
Thread per socket p2p
    @param arg: porta destinatario
*/
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
        if(errno == ECONNREFUSED) {
            fprintf(stderr, "Connessione rifiutata dalla porta %d.\n", porta_destinatario);
        } else {
            printf("Errore nella connessione P2P.\n");
        }
        close(sd_p2p);
        return NULL;
    }
    /* Invio della review */
    char review_msg[64];
    sprintf(review_msg, "REVIEW_CARD%s%d", CARATTERE_SEPARATORE, card_assegnata.id);
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
        /* Essendo che ci sono più thread che possono modificare review_ricevute, uso un mutex */
        pthread_mutex_lock(&ascolto);
        card_assegnata.review_ricevute++;
        pthread_mutex_unlock(&ascolto);
        printf("ACK_REVIEW_CARD ricevuto dalla porta %d.\n", porta_destinatario);
    }
    close(sd_p2p);
    return NULL;
}

/*
Funzione per gestire il comando REVIEW_CARD 
    @param sd: socket
*/
void review_card_function(int sd) {
    if(!hello_eseguito) {
        printf("Non sei connesso alla lavagna, esegui il comando HELLO.\n");
        return;
    }

    if(card_assegnata.id < 0) {
        printf("Non hai nessuna card assegnata.\n");
        return;
    }

    /* Richiedo la lista degli utenti connessi (per evitare di avere in memoria dati obsoleti)*/
    request_user_list_function(sd);
    sleep(1);  /* Attendo che la lista venga salvata */

    if(card_assegnata.num_utenti < 1){
        printf("Il numero di utenti connessi è troppo basso per inviare review.\n");
        return;
    }

    card_assegnata.review_ricevute = 0;

    /* Invio REVIEW_CARD a tutti gli utenti connessi attraverso thread*/
    for(int i = 0; i < card_assegnata.num_utenti; i++) {
        int porta_destinatario = card_assegnata.porte_utenti[i];

        pthread_t p2p_socket;
        pthread_create(&p2p_socket, NULL, gestione_p2p_review, (void *)(intptr_t)porta_destinatario);
        pthread_detach(p2p_socket);
    }

    /* Attendo un tempo proporzionale al numero utenti */
    int tempo_attesa = (card_assegnata.num_utenti / 2) + 1;
    sleep(tempo_attesa);

    if(card_assegnata.review_ricevute == card_assegnata.num_utenti) {
        printf("REVIEW_CARD ricevute.\n");
    } else {
        printf("Non tutte le review sono state ricevute.\n");
    }
}

/* 
Thread server P2P per ricevere REVIEW_CARD 
    @param arg: porta utente
*/
void* server_p2p(void* arg) {
    int porta_utente = (int)(intptr_t)arg;

    int sd_server, sd_client;
    struct sockaddr_in addr_server, addr_client;
    socklen_t len_client = sizeof(addr_client);

    /* Creazione socket server */
    sd_server = socket(AF_INET, SOCK_STREAM, 0);
    if(sd_server < 0) {
        perror("Errore nella creazione socket server P2P");
        connessione_attiva = 0;
        return NULL;
    }

    /* Creazione indirizzo server P2P */
    memset(&addr_server, 0, sizeof(addr_server));
    addr_server.sin_family = AF_INET;
    addr_server.sin_port = htons(porta_utente);
    if(inet_pton(AF_INET, "127.0.0.1", &addr_server.sin_addr) <= 0) {
        perror("Errore nella conversione dell'indirizzo IP server P2P");
        connessione_attiva = 0;
        close(sd_server);
        return NULL;
    }

    /* Binding del socket server P2P */
    if(bind(sd_server, (struct sockaddr *)&addr_server, sizeof(addr_server)) < 0) {
        if(errno == EADDRINUSE){
            fprintf(stderr,"Errore: la porta %d è già in uso.\n", porta_utente);
        }
        else {
            perror("Errore nel binding del socket server P2P");
        }
        connessione_attiva = 0;
        close(sd_server);
        exit(EXIT_FAILURE);
        return NULL;
    }

    listen(sd_server, 10);

    char buffer[64];

    while(1) {
        /* Accetto connessione in arrivo */
        if((sd_client = accept(sd_server, (struct sockaddr *)&addr_client, &len_client)) < 0) {
            perror("Errore nell'accept P2P");
            break;
        }

        /* Ricevi REVIEW_CARD */
        if(recv_message(sd_client, buffer, sizeof(buffer)) > 0) {
            if(strncmp(buffer, "REVIEW_CARD|", 12) == 0) {
                printf("REVIEW_CARD della card ID %s ricevuta.\n", buffer + 12);
                printf("Inviando ACK_REVIEW_CARD...\n");
                
                /* Invio ACK_REVIEW_CARD */
                char ack_msg[] = "ACK_REVIEW_CARD";
                if(send_message(sd_client, ack_msg) < 0) {
                    perror("Errore nell'invio dell'ACK_REVIEW_CARD");
                }
                printf("ACK_REVIEW_CARD inviato.\n");
                close(sd_client);
            }
        }
    }
    close(sd_server);
    return NULL;
}


/*
* ============================================================================ *
* ============================================================================ *
                            GESTIONE ricezione da lavagna
* ============================================================================ *
* ============================================================================ *
*/

/*Thread per gestire i messaggi in arrivo dalla lavagna*/
void* gestione_ascolto(void* arg) {
    int sd = (int)(intptr_t)arg;
    char buffer[8192];
    
    while(1) {
        if(recv_message(sd, buffer, sizeof(buffer) - 1) <= 0) {
            if(errno == ECONNRESET) {
                fprintf(stderr, "Connessione chiusa bruscamente dalla lavagna.\n");
            } else {
                printf("Connessione chiusa dalla lavagna.\n");
            }
            connessione_attiva = 0;
            break;
        }
    
        /* Registrazione HELLO */
        if (strncmp(buffer, "Registrazione avvenuta con successo.", 36) == 0) {
            hello_eseguito = 1;
        }
        else if(strncmp(buffer, "Errore nell'ack della card.", 33) == 0) {
            card_assegnata.id = -1;  /* Reset card assegnata */
        }
        /* Lista utenti ricevuta */
        else if (strncmp(buffer, "SEND_USER_LIST", 14) == 0) {
            save_user_list(buffer + 15);
            continue;
        }
        /* Nuova card assegnata */
        else if(strncmp(buffer,"HANDLE_CARD",11)==0){
            handle_card_function(buffer + 12);
            continue;
        }
        /* Risposta al ping */
        else if(strncmp(buffer, "PING", 4) == 0){
            ping_ricevuto = 1;
            printf("\n>>> PING ricevuto dalla lavagna.\n\n");
            continue;
        }
        /* SHOW_LAVAGNA */
        else if(strncmp(buffer, "LAVAGNA_STATE",13) == 0){
            print_lavagna(buffer + 14);
            continue;
        }

        /* Messaggi generici */
        printf("\n[LAVAGNA] %s\n", buffer);

    }
    
    close(sd);
    return NULL;
}



/*
* ============================================================================ *
* ============================================================================ *
                            MAIN UTENTE - GESTIONE COMANDI
* ============================================================================ *
* ============================================================================ *
*/
int main(int argc, char *argv[]) {
    pthread_mutex_init(&ascolto, NULL);
    
    if (argc != 2) {
        printf("L'eseguibile richiedela porta.\n");
        return -1;
    }
    
    porta_utente = atoi(argv[1]);

    /*Controllo che la porta inserita sia maggiore di PORTA_LAVAGNA*/
    if (porta_utente <= PORTA_LAVAGNA) {
        printf("La porta deve essere maggiore di %d.\n", PORTA_LAVAGNA);
        return -1;
    }

    /*Creazione socket*/
    int sd, ret;
    struct sockaddr_in lavagna_addr;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) {
        perror("Errore nella creazione della socket");
        exit(EXIT_FAILURE);
    }

    /* Creazione indirizzo*/
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
        if(errno == ECONNREFUSED){
            fprintf(stderr,"Lavagna non attiva. Aspettare che la lavagna venga avviata e riprovate.\n");
        }
        else {
            perror("Errore nella connessione alla lavagna");
        }
        close(sd);
        exit(EXIT_FAILURE);
    }

    printf("Connessione alla lavagna avvenuta con successo.\n");
    connessione_attiva = 1;

    /* Server P2P per ricevere REVIEW_CARD */
    pthread_t thread_server_p2p;
    if(pthread_create(&thread_server_p2p, NULL, server_p2p, (void *)(intptr_t)porta_utente) != 0) {
        perror("Errore nella creazione del thread server P2P");
        close(sd);
        exit(EXIT_FAILURE);
    }
    pthread_detach(thread_server_p2p);
    
    /*Creazione thread per ascoltare dati in arrivo */
    pthread_t thread_ascolto;
    if(pthread_create(&thread_ascolto, NULL, gestione_ascolto, (void *)(intptr_t)sd) != 0) {
        perror("Errore nella creazione del thread di ascolto");
        close(sd);
        exit(EXIT_FAILURE);
    }
    pthread_detach(thread_ascolto);

    /* Controllo che i comandi inseriti da tastiera siano quelli permessi.*/
    char comando[20];

    sleep(1);
    while (connessione_attiva){        
        printf("Inserisci il comando:\n");
        if(fgets(comando, sizeof(comando), stdin) == NULL) {
            perror("Errore nella lettura del comando");
            continue;
        }
        if(strchr(comando, '\n') == NULL) {
            printf("Errore: il comando è troppo lungo (max 19 caratteri).\n");
            int c;
            while((c = getchar()) != '\n' && c != EOF);
            continue;
        }
        comando[strcspn(comando, "\n")] = 0;

        printf(RIGA_SEPARATORIA);

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
        else if(strcmp(comando, "SHOW_LAVAGNA") == 0) {
            show_lavagna_function(sd);
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
        else if(strcmp(comando, "PONG_LAVAGNA") == 0) {
            pong_lavagna_function(sd);
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