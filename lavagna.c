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
#include "strutture_lavagna.h"
#include "funzioni_x_msg.h"             // Per send_message e recv_message

/*Definizione funzioni per le chiamate*/
int registrazione_utente(int porta_utente, int sd_utente);
int rimozione_utente(int porta_utente);
int controllo_stato(const char *str);
int create_card(const char* dati, int porta_utente);
int move_card(int id_card, STATO_COLONNA vecchia_colonna, STATO_COLONNA nuova_colonna);
int ack_card(int id_card, int porta_utente);
int card_done(int id_card, int porta_utente);
void card_doing_check(int porta_utente);
int send_user_list(int sd, int porta_destinatario);
int send_show_lavagna(int sd_utente);
void handle_card();
void show_lavagna();
void creazione_lavagna();


/*
* ============================================================================ *
* ============================================================================ *
                                GESTIONE UTENTI
* ============================================================================ *
* ============================================================================ *
*/

/*
Funzione per la registrazione di un utente
    @param porta_utente: porta dell'utente da registrare
    @param sd_utente: socket dell'utente da registrare
*/
int registrazione_utente(int porta_utente, int sd_utente){
    
    /* Controllo se la porta è già registrata */
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(lavagna->utenti_connessi[i].porta_utente == porta_utente){
            return -1;
        }
    }

    /* Aggiungo in coda l'utente*/
    lavagna->numero_utenti_connessi++;
    lavagna->utenti_connessi = realloc(lavagna->utenti_connessi, 
        lavagna->numero_utenti_connessi * sizeof(struct INFO_UTENTE));

    if(lavagna->utenti_connessi == NULL){
        perror("Errore nella registrazione dell'utente");
        exit(EXIT_FAILURE);
    }

    /*Inizializzo/salvo i dati dell'utente*/
    lavagna->utenti_connessi[lavagna->numero_utenti_connessi - 1].porta_utente = porta_utente;
    lavagna->utenti_connessi[lavagna->numero_utenti_connessi - 1].socket_id = sd_utente;
    lavagna->utenti_connessi[lavagna->numero_utenti_connessi - 1].occupato = 0;
    lavagna->utenti_connessi[lavagna->numero_utenti_connessi - 1].pong_ricevuto = 0;

    return 0;
}


/*
Funzione per la rimozione di un utente in seguito alla QUIT
    @param porta_utente: porta dell'utente da rimuovere
*/
int rimozione_utente(int porta_utente){
    int trovato = 0;

    /*Devo controllare che l'utente non abbia card in corso*/
    card_doing_check(porta_utente);
    
    /*Cerco l'utente e lo rimuovo dalla lista*/
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(lavagna->utenti_connessi[i].porta_utente == porta_utente) {
            trovato = 1;

            /*Azzero i dati dell'utente*/
            lavagna->utenti_connessi[i].socket_id = 0;
            lavagna->utenti_connessi[i].occupato = 0;
            lavagna->utenti_connessi[i].porta_utente = 0;
            lavagna->utenti_connessi[i].pong_ricevuto = 0;
        }
        if(trovato && i < lavagna->numero_utenti_connessi - 1) {
            lavagna->utenti_connessi[i] = lavagna->utenti_connessi[i + 1];
        }
    }

    /*Se ho trovato l'utente rialloco le porte*/
    if(trovato){
        lavagna->numero_utenti_connessi--;
        lavagna->utenti_connessi = realloc(lavagna->utenti_connessi, 
            lavagna->numero_utenti_connessi * sizeof(struct INFO_UTENTE));

        if(lavagna->numero_utenti_connessi > 0 && lavagna->utenti_connessi == NULL){
            perror("Errore nella rimozione dell'utente");
            exit(EXIT_FAILURE);
        }

    }

    return 0;
}

/* 
Thread per eseguire il ping ad un utente
    @param arg: porta utente da pingare
*/
void* ping_user(void* arg){
    int porta_utente = (int)(intptr_t)arg;
    int socket_utente = -1;

    /* Trovo il socket dell'utente dalla porta */
    pthread_mutex_lock(&accesso_lavagna);
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(lavagna->utenti_connessi[i].porta_utente == porta_utente){
            socket_utente = lavagna->utenti_connessi[i].socket_id;
            break;
        }
    }
    pthread_mutex_unlock(&accesso_lavagna);

    /* Controllo se ho trovato il socket */
    if(socket_utente == -1){
        printf("Impossibile trovare il socket per la porta %d durante il ping.\n", porta_utente);
        return NULL;
    }

    /* Invio il ping */
    char msg[] = "PING";
    if(send_message(socket_utente, msg) < 0){
        perror("Errore nell'invio del ping all'utente");
        rimozione_utente(porta_utente);
        close(socket_utente);
        return NULL;
    }

    printf("Ping inviato all'utente sulla porta %d.\n", porta_utente);

    /* Attendo 30 secondi per il pong */
    sleep(30);

    /* Controllo se è stato ricevuto il pong */
    pthread_mutex_lock(&accesso_lavagna);
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(lavagna->utenti_connessi[i].porta_utente == porta_utente){

            if(lavagna->utenti_connessi[i].pong_ricevuto == 0){
                printf("Nessun pong ricevuto dall'utente sulla porta %d. Procedo alla rimozione.\n", porta_utente);
                rimozione_utente(porta_utente);
                close(socket_utente);
            } else {
                /* Resetto il flag per il prossimo ping */
                lavagna->utenti_connessi[i].pong_ricevuto = 0;
            }
            break;
        }
    }
    pthread_mutex_unlock(&accesso_lavagna);
    return NULL;
}

/*
Thread per il ping degli utenti connessi
    @param arg: non usato
*/
void* handler_ping_users(void* arg){
    while(1){
        sleep(35);

        pthread_mutex_lock(&accesso_lavagna);
        time_t tempo_corrente = time(NULL);

        for(int i = 0; i < lavagna->colonne[DOING].numero_card; i++){
            struct st_CARD *card = &lavagna->colonne[DOING].cards[i];

            if(tempo_corrente - card->timestamp_ultima_modifica > 90){
                card->timestamp_ultima_modifica = tempo_corrente;
                pthread_t thread_ping;
                pthread_create(&thread_ping, NULL, ping_user, (void*)(intptr_t)card->porta_utente);
                pthread_detach(thread_ping);
            }
        }
        pthread_mutex_unlock(&accesso_lavagna);
    }
    return NULL;
}


/*
* ============================================================================ *
* ============================================================================ *
                                GESTIONE CARD
* ============================================================================ *
* ============================================================================ *
*/

/*
Funzione per controllo valore colonna passato
    @param str: stringa rappresentante lo stato della colonna
*/
int controllo_stato(const char *str) {
    if(strcmp(str, "TO_DO") == 0) 
        return TO_DO;
    if(strcmp(str, "DOING") == 0) 
        return DOING;
    if(strcmp(str, "DONE") == 0) 
        return DONE;
    return -1; 
}

/*
Funzione per controllare e spostare le card in TO_DO di un utente che si disconnette
    @param porta_utente: porta dell'utente
*/
void card_doing_check(int porta_utente){
    int colonna = (int)DOING;

    for(int i = 0; i < lavagna->colonne[colonna].numero_card; i++){
        if(lavagna->colonne[colonna].cards[i].porta_utente == porta_utente){
            int id_card = lavagna->colonne[colonna].cards[i].id;
            lavagna->colonne[colonna].cards[i].porta_utente = 0;
            lavagna->colonne[colonna].cards[i].timestamp_ultima_modifica = time(NULL);
            move_card(id_card, DOING, TO_DO);
            i--; 
        }
    }
}

/*
Funzione per la creazione di una nuova card
    @param dati: stringa contenente i dati della card (id|colonna|testo)
    @param porta_utente: porta dell'utente che crea la card
*/
int create_card(const char* dati, int porta_utente){
    /* Parsing dei dati */
    char card_copia[512];
    strncpy(card_copia, dati, sizeof(card_copia));
    card_copia[sizeof(card_copia) - 1] = '\0';

    /* Estraggo i campi della card */
    char *elem = strtok(card_copia, CARATTERE_SEPARATORE);
    int id_card = atoi(elem);
    elem = strtok(NULL, CARATTERE_SEPARATORE);
    STATO_COLONNA colonna = controllo_stato(elem);
    if(colonna == -1){
        return -1;
    }
    elem = strtok(NULL, CARATTERE_SEPARATORE);
    char *testo = elem;

    /*Verifico che l'id della card non sia già presente nella Lavagna*/
    for(int i = 0; i < 3; i++){
        for(int j = 0; j < lavagna->colonne[i].numero_card; j++){
            if(lavagna->colonne[i].cards[j].id == id_card){
                return -1;
            }
        }
    }

    /* Creazione della nuova card */
    struct st_CARD new_card;
    new_card.id = id_card;
    new_card.colonna = colonna;
    strncpy(new_card.testo, testo, sizeof(new_card.testo));
    new_card.testo[sizeof(new_card.testo) - 1] = '\0';
    new_card.porta_utente = 0;
    new_card.timestamp_ultima_modifica = time(NULL);    

    /* Aggiunta della card alla colonna corrispondente */
    int col_index = (int)colonna;
    lavagna->colonne[col_index].numero_card++;
    lavagna->colonne[col_index].cards = realloc(lavagna->colonne[col_index].cards, 
        lavagna->colonne[col_index].numero_card * sizeof(struct st_CARD));
    if(lavagna->colonne[col_index].cards == NULL){
        perror("Errore nella creazione della card");
        exit(EXIT_FAILURE);
    }
    lavagna->colonne[col_index].cards[lavagna->colonne[col_index].numero_card - 1] = new_card;
    lavagna->numero_card_totali++;
    
    return 0;
}


/*
Funzione per spostare una card da una colonna ad un'altra
    @param id_card: id della card da spostare
    @param vecchia_colonna: colonna di partenza
    @param nuova_colonna: colonna di arrivo
*/
int move_card(int id_card, STATO_COLONNA vecchia_colonna, STATO_COLONNA nuova_colonna){
    /* Cerco la card nella lavagna */
    int vecchia_col_id = (int)vecchia_colonna;
    int nuova_col_id = (int)nuova_colonna;
    struct st_CARD card_select;
    int posizione_card = -1;


    for(int i = 0; i < lavagna->colonne[vecchia_col_id].numero_card; i++){
        if(lavagna->colonne[vecchia_col_id].cards[i].id == id_card){
            card_select = lavagna->colonne[vecchia_col_id].cards[i];
            posizione_card = i;
            break;
        }
    }
    if(posizione_card == -1){
        return -1;
    }

    /* Rimuovo la card dalla vecchia colonna */
    for(int j = posizione_card; j < lavagna->colonne[vecchia_col_id].numero_card - 1; j++){
        lavagna->colonne[vecchia_col_id].cards[j] = lavagna->colonne[vecchia_col_id].cards[j + 1];
    }
    lavagna->colonne[vecchia_col_id].numero_card--;
    lavagna->colonne[vecchia_col_id].cards = realloc(lavagna->colonne[vecchia_col_id].cards, 
        lavagna->colonne[vecchia_col_id].numero_card * sizeof(struct st_CARD));

    /* Controllo realloc */
    if(lavagna->colonne[vecchia_col_id].numero_card > 0 && lavagna->colonne[vecchia_col_id].cards == NULL){
        perror("Errore nello spostamento della card");
        exit(EXIT_FAILURE);
    }

    /* Aggiungo la card alla nuova colonna */
    lavagna->colonne[nuova_col_id].numero_card++;
    lavagna->colonne[nuova_col_id].cards = realloc(lavagna->colonne[nuova_col_id].cards, 
        lavagna->colonne[nuova_col_id].numero_card * sizeof(struct st_CARD));
    if(lavagna->colonne[nuova_col_id].cards == NULL){
        perror("Errore nello spostamento della card");
        exit(EXIT_FAILURE);
    }
    lavagna->colonne[nuova_col_id].cards[lavagna->colonne[nuova_col_id].numero_card - 1] = card_select;      

    show_lavagna();
    pthread_cond_signal(&lavagna_aggiornata);
    return 0;
}


/*
Funzione per spostare una card da TO_DO a DOING in seguito ad un ack
    @param id_card: id della card da spostare
    @param porta_utente: porta dell'utente che ha fatto l'ack
*/
int ack_card(int id_card, int porta_utente){
    /* Trova la card in TO_DO e salva la porta */
    for(int i = 0; i < lavagna->colonne[TO_DO].numero_card; i++){
        if(lavagna->colonne[TO_DO].cards[i].id == id_card){
            lavagna->colonne[TO_DO].cards[i].porta_utente = porta_utente;
            lavagna->colonne[TO_DO].cards[i].timestamp_ultima_modifica = time(NULL);

            /* Sposto la card da TO_DO a DOING */
            if(move_card(id_card, TO_DO, DOING) != 0){
                return -1;
            }

            return 0;
        }
    }

    return -1;
}

/*
Funzione per spostare una card da DOING a DONE in seguito al completamento
    @param id_card: id della card da spostare
    @param porta_utente: porta dell'utente che ha inviato il comando di completamento
*/
int card_done(int id_card, int porta_utente){
    /*Trovo la card in DOING e salvo la porta e il timestamp*/
    for(int i = 0; i < lavagna->colonne[DOING].numero_card; i++){
        if(lavagna->colonne[DOING].cards[i].id == id_card){
            lavagna->colonne[DOING].cards[i].porta_utente = porta_utente;
            lavagna->colonne[DOING].cards[i].timestamp_ultima_modifica = time(NULL);
            break;
        }
    }

    if(move_card(id_card, DOING, DONE) != 0){
        return -1;
    }

    /*Una volta arrivato l'utente non è più occupato*/
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(lavagna->utenti_connessi[i].porta_utente == porta_utente){
            lavagna->utenti_connessi[i].occupato = 0;
            break;
        }
    }
    return 0;
}


/*
* ============================================================================ *
* ============================================================================ *
                        GESTIONE COMUNICAZIONI CON UTENTI
* ============================================================================ *
* ============================================================================ *
*/

/*Funzione per inviare la lista delle porte salvate*/
int send_user_list(int sd, int porta_destinatario){
    char lista[1024];

    /*Inserisco numero utenti (meno 1 del destinatario)*/
    int pos = 0;
    
    pos += snprintf(lista + pos, sizeof(lista) - pos, "SEND_USER_LIST%s%d", CARATTERE_SEPARATORE, 
                    lavagna->numero_utenti_connessi - 1);
    

    /*Inserisco lista porte utenti (escludendo la porta del destinatario)*/
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(lavagna->utenti_connessi[i].porta_utente == porta_destinatario){
            continue;
        }
        pos += snprintf(lista + pos, sizeof(lista) - pos, "%s%d", CARATTERE_SEPARATORE,
                        lavagna->utenti_connessi[i].porta_utente);
    }

    if(send_message(sd, lista) < 0){
        perror("Errore nell'invio della lista utenti");
        rimozione_utente(porta_destinatario);
        close(sd);
        return -1;
    }

    return 0;
}


/* Funzione per inviare lo stato attuale della lavagna ad un utente
    @param sd_utente: socket dell'utente a cui inviare lo stato della lavagna
*/
int send_show_lavagna(int sd_utente){
    char msg[8192];
    int pos = 0;

    /* Formato: LAVAGNA_STATE|id_lavagna|max_cards||card1_col1|card2_col1|...||card1_col2|card2_col2...|| */

    /* Aggiungo id lavagna */
    pos += snprintf(msg + pos, sizeof(msg) - pos, "LAVAGNA_STATE%s%d%s", CARATTERE_SEPARATORE, lavagna->id, CARATTERE_SEPARATORE);
    
    int max_cards = 0;
    for(int i = 0; i < NUM_COLONNE; i++){
        if(lavagna->colonne[i].numero_card > max_cards){
            max_cards = lavagna->colonne[i].numero_card;
        }
    }
    
    /* Aggiungo numero massimo di card */
    pos += snprintf(msg + pos, sizeof(msg) - pos, "%d%s%s", max_cards, CARATTERE_SEPARATORE, CARATTERE_SEPARATORE);
    
    for(int col = 0; col < NUM_COLONNE; col++){
        int cards_scritte = 0;
        
        for(int i = 0; i < lavagna->colonne[col].numero_card; i++){ 
            struct st_CARD card = lavagna->colonne[col].cards[i];

            if(cards_scritte > 0){
                pos += snprintf(msg + pos, sizeof(msg) - pos, "%s", CARATTERE_SEPARATORE);
            }

            pos += snprintf(msg + pos, sizeof(msg) - pos, "%d%s%s", card.id, CARATTERE_SEPARATORE, card.testo);
            cards_scritte++;
        }
        if(col < NUM_COLONNE - 1){
            /* Inserisco || per separare le colonne */
            pos += snprintf(msg + pos, sizeof(msg) - pos, "%s%s", CARATTERE_SEPARATORE, CARATTERE_SEPARATORE);
        }
    }

    if(send_message(sd_utente, msg) < 0){
        perror("Errore nell'invio dello stato della lavagna");
        return -1;
    }
    return 0;
}


/* Funzione per l'attribuzione di card agli utenti connessi */
void handle_card(){

    int colonna_to_do = (int)TO_DO;

    for(int i = 0; i < lavagna->colonne[colonna_to_do].numero_card; i++){

        struct st_CARD card_selezionata = lavagna->colonne[colonna_to_do].cards[i];

        /* Assegno la card al primo utente disponibile (occupato = 0) */
        for(int j = 0; j < lavagna->numero_utenti_connessi; j++){

            if(lavagna->utenti_connessi[j].occupato == 0){
                /* Costruisco il messaggio con lista utenti (escluso destinatario) */
                char msg[1024];
                int off = 0;
                int num_utenti_escl = lavagna->numero_utenti_connessi - 1;
                
                /* Inizio messaggio con ID, testo e numero utenti */
                off += snprintf(msg, sizeof(msg) - off, "HANDLE_CARD%s%d%s%s%s%d", CARATTERE_SEPARATORE,
                    card_selezionata.id, CARATTERE_SEPARATORE, card_selezionata.testo, CARATTERE_SEPARATORE, num_utenti_escl);
                
                /* Aggiungo lista porte (escludendo il destinatario alla posizione j) */
                for(int k = 0; k < lavagna->numero_utenti_connessi; k++){
                    if(k != j){
                        off += snprintf(msg + off, sizeof(msg) - off, "%s%d", CARATTERE_SEPARATORE, lavagna->utenti_connessi[k].porta_utente);
                    }
                }
                
                /* Invio la card all'utente */
                if(send_message(lavagna->utenti_connessi[j].socket_id, msg) < 0){
                    perror("Errore nell'invio della card all'utente");
                }

                /* Segno l'utente come occupato */
                lavagna->utenti_connessi[j].occupato = 1;
                break;
            }
        }
    }

    printf("Card inviate, attesa ACK da parte degli utenti.\n");

    return;
}

/*
Thread per il broadcast dello stato della lavagna agli utenti
    @param arg: non usato
*/
void* lavagna_broadcast(void* arg){
    pthread_mutex_lock(&accesso_lavagna);
    while(1){
        pthread_cond_wait(&lavagna_aggiornata, &accesso_lavagna);
        for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
            int sd_utente = lavagna->utenti_connessi[i].socket_id;
            if(sd_utente < 0){
                continue;
            }
            if(send_show_lavagna(sd_utente) != 0){
                printf("Errore nell'invio dello stato della lavagna alla porta %d.\n", 
                    lavagna->utenti_connessi[i].porta_utente);
            }
        }
    }
    pthread_mutex_unlock(&accesso_lavagna);
    return NULL;
}


/*
* ============================================================================ *
* ============================================================================ *
                            GESTIONE LAVAGNA
* ============================================================================ *
* ============================================================================ *

*/

/*Funzione per la visualizzazione della lavagna*/
void show_lavagna(){
    printf("\n");
    printf("----------------------------------------------------------------\n");
    printf("|                         Lavagna - %d                          |\n", lavagna->id);
    printf("----------------------------------------------------------------\n");
    printf("|        TO DO       |        DOING       |        DONE        |\n");
    printf("----------------------------------------------------------------\n");
    
    int max_cards = 0;
    for(int i = 0; i < NUM_COLONNE; i++){
        if(lavagna->colonne[i].numero_card > max_cards){
            max_cards = lavagna->colonne[i].numero_card;
        }
    }
    /* Stampa ogni card: ID su una riga, testo su quella dopo */
    for(int i = 0; i < max_cards; i++){
        /* Riga 1: ID */
        for(int j = 0; j < NUM_COLONNE; j++){
            if(i < lavagna->colonne[j].numero_card){
                struct st_CARD card = lavagna->colonne[j].cards[i];
                printf("|        ID: %-8d", card.id);
            } else {
                printf("|                    ");
            }
        }
        printf("|\n");
        
        /* Riga 2: Testo con wrapping */
        int fine = 0;
        int line = 0;
        while (fine == 0) {
            for(int j = 0; j < NUM_COLONNE; j++){
                if(i < lavagna->colonne[j].numero_card){
                    struct st_CARD card = lavagna->colonne[j].cards[i];
                    
                    int start = line * 18;
                    if(start < strlen(card.testo)){
                        char segmento[19];
                        strncpy(segmento, &card.testo[start], 18);
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
            int has_text = 0;
            for(int j = 0; j < NUM_COLONNE; j++){
                if(i < lavagna->colonne[j].numero_card){
                    struct st_CARD card = lavagna->colonne[j].cards[i];
                    int start = line * 18;
                    if(start < strlen(card.testo)){
                        has_text = 1;
                        break;
                    }
                }
            }
            if(!has_text){
                fine = 1;
            }
        }
        printf("----------------------------------------------------------------\n");
    }

    printf("\n");
    return;
}

/*Funzione per la creazione della lavagna e inizializzazione*/
void creazione_lavagna(){
    /*Inizializzazione semafori e var condition*/
    pthread_mutex_init(&accesso_lavagna, NULL);
    pthread_cond_init(&lavagna_aggiornata, NULL);

    /*Creazione lavagna*/
    lavagna = malloc(sizeof(struct st_LAVAGNA));

    if(lavagna == NULL){
        perror("Errore nell'allocazione della lavagna");
        exit(EXIT_FAILURE);
    }

    lavagna->id = 1;
    lavagna->numero_utenti_connessi = 0;
    lavagna->numero_card_totali = 0;
    lavagna->utenti_connessi = NULL;

    for(int i = 0; i < NUM_COLONNE; i++){
        lavagna->colonne[i].stato = (STATO_COLONNA)i;
        lavagna->colonne[i].numero_card = 0;
        lavagna->colonne[i].cards = NULL;
    }

    /*Caricamento card da file*/
    FILE * file = fopen("card.txt", "r");
    if (file != NULL) {
        char line[64];
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\r\n")] = 0; 

            char *elem = strtok(line, ":");
            int id_card = atoi(elem);
            elem = strtok(NULL, ":");
            char *testo = elem;

            struct st_CARD new_card;
            new_card.id = id_card;
            new_card.colonna = TO_DO;
            strncpy(new_card.testo, testo, sizeof(new_card.testo));
            new_card.testo[sizeof(new_card.testo) - 1] = '\0';
            new_card.porta_utente = 0;
            new_card.timestamp_ultima_modifica = time(NULL);    

            lavagna->colonne[TO_DO].numero_card++;
            lavagna->colonne[TO_DO].cards = realloc(lavagna->colonne[TO_DO].cards, 
                lavagna->colonne[TO_DO].numero_card * sizeof(struct st_CARD));
            if(lavagna->colonne[TO_DO].cards == NULL){
                perror("Errore nella creazione della card da file");
                exit(EXIT_FAILURE);
            }
            lavagna->colonne[TO_DO].cards[lavagna->colonne[TO_DO].numero_card - 1] = new_card;
            lavagna->numero_card_totali++;
        }
    }
    fclose(file);

    printf("Lavagna creata e pronta a ricevere connessioni.\n");
}


/*
* ============================================================================ *
* ============================================================================ *
                    THREAD GESTIONE COMANDI LAVAGNA E UTENTI
* ============================================================================ *
* ============================================================================ *
*/


void* gestione_lavagna(void* arg){
    char comando[50];

    while(1){
        if(fgets(comando, sizeof(comando), stdin) == NULL) {
            printf("Errore nella lettura del comando.\n");
            continue;
        }
        if(strchr(comando, '\n') == NULL) {
            printf("Errore: il comando è troppo lungo (max 49 caratteri).\n");
            int c;
            while((c = getchar()) != '\n' && c != EOF);
            continue;
        }
        comando[strcspn(comando, "\n")] = 0; 

        /*Gestione comandi*/
        /*Comando SHOW_LAVAGNA*/
        if(strcmp(comando, "SHOW_LAVAGNA") == 0){
            pthread_mutex_lock(&accesso_lavagna);
            show_lavagna();
            pthread_mutex_unlock(&accesso_lavagna);
            continue;
        } 
        /*Comando HANDLE_CARD*/
        else if(strcmp(comando, "HANDLE_CARD") == 0) {
            pthread_mutex_lock(&accesso_lavagna);
            handle_card();
            pthread_mutex_unlock(&accesso_lavagna);
            continue;
        }
        /*Comando SEND_USER_LIST*/
        else if (strcmp(comando, "SEND_USER_LIST")==0) {
            pthread_mutex_lock(&accesso_lavagna);
            for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
                int sd_utente = lavagna->utenti_connessi[i].socket_id;
                int porta_utente = lavagna->utenti_connessi[i].porta_utente;
                if(send_user_list(sd_utente, porta_utente) != 0){
                    printf("Errore nell'invio della lista utenti alla porta %d.\n", porta_utente);
                } else {
                    printf("Lista utenti inviata con successo alla porta %d.\n", porta_utente);
                }
            }
            pthread_mutex_unlock(&accesso_lavagna);
            continue;
        }
        else {
            printf("Comando non riconosciuto.\n");
        }
    }

    return NULL;
}



void* gestione_utente(void* arg){
    int sd_utente = (int)(intptr_t)arg;
    char buffer[512];
    int porta_utente = 0;

    /*Arrivo comandi da parte dell'utente*/
    while(1){
        /*Ricezione messaggio*/
        if(recv_message(sd_utente, buffer, sizeof(buffer)) < 0) {
            if(errno == ECONNRESET) {
                fprintf(stderr, "Utente sulla porta %d ha chiuso bruscamente la connessione\n", porta_utente);
            } else {
                if(porta_utente != 0)
                    printf("Connessione utente sulla porta %d interrotta.\n", porta_utente);
                else
                    printf("Connessione utente sconosciuto (non ha eseguito HELLO) interrotta.\n");
            }
            rimozione_utente(porta_utente);
            close(sd_utente);
            break;
        }

        printf(RIGA_SEPARATORIA);
        printf("Comando ricevuto dall'utente: %s\n", buffer);

        /*Gestione comandi*/
        /*Comando HELLO*/
        if (strncmp(buffer, "HELLO", 5) == 0) {
            porta_utente = atoi(&buffer[6]);

            pthread_mutex_lock(&accesso_lavagna);
            if (registrazione_utente(porta_utente, sd_utente) != 0) {
                const char *risposta = "Porta già registrata.";
                send_message(sd_utente, risposta);
            }
            else{
                printf("Utente alla porta %d registrato.\n\n", porta_utente);
                const char *risposta = "Registrazione avvenuta con successo.";
                send_message(sd_utente, risposta);
            }
            
            pthread_mutex_unlock(&accesso_lavagna);   
        }
        
        /*Comando SHOW_LAVAGNA*/
        else if (strcmp(buffer, "SHOW_LAVAGNA") == 0) {

            pthread_mutex_lock(&accesso_lavagna);
            if(send_show_lavagna(sd_utente) != 0){
                const char *risposta = "Errore nella visualizzazione della lavagna.";
                send_message(sd_utente, risposta);
            }
            printf("%d>>> Stato lavagna inviato con successo.\n\n", porta_utente);
            
            pthread_mutex_unlock(&accesso_lavagna);
        }

        /*Comando QUIT*/
        else if (strncmp(buffer, "QUIT", 4) == 0) {
            printf("%d>>> Utente disconnesso.\n\n", porta_utente);
            pthread_mutex_lock(&accesso_lavagna);
            rimozione_utente(porta_utente);
            pthread_mutex_unlock(&accesso_lavagna);
            close(sd_utente);
            break;
        }

        /*Comando CREATE_CARD*/
        else if (strncmp(buffer, "CREATE_CARD", 11) == 0){
            printf("%d>>> Comando CREATE_CARD ricevuto.\n\n", porta_utente);

            pthread_mutex_lock(&accesso_lavagna);
            if(create_card(buffer + 12, porta_utente) != 0){
                const char *risposta = "Errore nella creazione della card (ID possibile duplicato).";
                send_message(sd_utente, risposta);
            } 
            else {
                const char *risposta = "Card creata con successo.";
                send_message(sd_utente, risposta);                
            }

            pthread_mutex_unlock(&accesso_lavagna);
        }

        /*Comando ACK_CARD*/
        else if(strncmp(buffer, "ACK_CARD", 8) == 0){
            printf("%d>>> Comando ACK_CARD ricevuto.\n\n", porta_utente);

            int id = atoi(buffer + 9);
            pthread_mutex_lock(&accesso_lavagna);
            if(ack_card(id, porta_utente) != 0){
                const char *risposta = "Errore nell'ack della card.";
                send_message(sd_utente, risposta);

                /* Se l'ack fallisce, l'utente non deve essere visto come occupato */
                for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
                    if(lavagna->utenti_connessi[i].porta_utente == porta_utente){
                        lavagna->utenti_connessi[i].occupato = 0;
                        break;
                    }
                }
            }
            else {
                const char *risposta = "Card acked con successo.";
                send_message(sd_utente, risposta);                
            }

            pthread_mutex_unlock(&accesso_lavagna);
        }

        /*Comando REQUEST_USER_LIST*/
        else if (strcmp(buffer, "REQUEST_USER_LIST") == 0) {
            pthread_mutex_lock(&accesso_lavagna);
            if(send_user_list(sd_utente, porta_utente) != 0){
                const char *risposta = "Errore nell'invio della lista utenti.";
                send_message(sd_utente, risposta);
            }
            else {
                printf("%d>>> Lista utenti inviata con successo.\n\n", porta_utente);
            }

            pthread_mutex_unlock(&accesso_lavagna);
        }

        /*Comando CARD_DONE*/
        else if(strncmp(buffer, "CARD_DONE", 9) == 0){
            printf("%d>>> Comando CARD_DONE ricevuto.\n\n", porta_utente);

            int id = atoi(buffer + 10);
            pthread_mutex_lock(&accesso_lavagna);
            if(card_done(id, porta_utente) != 0){
                const char *risposta = "Errore nel completamento della card.";
                send_message(sd_utente, risposta);
            }
            else {
                const char *risposta = "Card completata con successo.";
                send_message(sd_utente, risposta);                
            }

            /* Una volta completata la card l'utente non è più occupato quindi riattribuisco una card */
            handle_card();

            pthread_mutex_unlock(&accesso_lavagna);
        }

        /*Comando PONG*/
        else if(strncmp(buffer, "PONG", 4) == 0){
            pthread_mutex_lock(&accesso_lavagna);
            for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
                if(lavagna->utenti_connessi[i].porta_utente == porta_utente){
                    lavagna->utenti_connessi[i].pong_ricevuto = 1;
                    printf("%d>>> Pong ricevuto.\n\n", porta_utente);
                    break;
                }
            }
            pthread_mutex_unlock(&accesso_lavagna);
        }
    }

    return NULL;
}



/*
* ============================================================================ *
* ============================================================================ *
                                MAIN LAVAGNA
* ============================================================================ *
* ============================================================================ *
*/
int main(){
    creazione_lavagna();
    show_lavagna();

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
    if(inet_pton(AF_INET, "127.0.0.1", &lavagna_addr.sin_addr) <= 0) {
        perror("Errore nella conversione dell'indirizzo IP");
        close(sd);
        exit(EXIT_FAILURE);
    }

    /* Binding del socket */
    if(bind(sd, (struct sockaddr *)&lavagna_addr, sizeof(lavagna_addr)) < 0) {
        if(errno == EADDRINUSE) {
            fprintf(stderr, "Errore: la porta %d è già in uso.\n", PORTA_LAVAGNA);
        } else {
            perror("Errore nel binding della socket");
        }
        close(sd);
        exit(EXIT_FAILURE);
    }
    /* Messa in ascolto del socket */
    listen(sd, MIN_UTENTI);

    /* Creazione thread ascolto comandi lavagna */
    pthread_t thread_lavagna;
    if(pthread_create(&thread_lavagna, NULL, gestione_lavagna, NULL) != 0){
        perror("Errore nella creazione del thread per la lavagna");
        close(sd);
        exit(EXIT_FAILURE);
    }
    pthread_detach(thread_lavagna);

    /* Creazione thread per PING_USER */
    pthread_t thread_ping;
    if(pthread_create(&thread_ping, NULL, handler_ping_users, NULL) != 0){
        perror("Errore nella creazione del thread per il ping degli utenti");
        close(sd);
        exit(EXIT_FAILURE);
    }
    pthread_detach(thread_ping);

    /* Creazione thread per invio status lavagna broadcast */
    pthread_t thread_broadcast;
    if(pthread_create(&thread_broadcast, NULL, lavagna_broadcast, NULL) != 0){
        perror("Errore nella creazione del thread per il broadcast dello status della lavagna");
        close(sd);
        exit(EXIT_FAILURE);
    }
    pthread_detach(thread_broadcast);

    /* Accettazione connessioni in arrivo */
    while (1)
    {
        printf(RIGA_SEPARATORIA);
        printf("In attesa di altre connessioni...\n");

        if((new_socket = accept(sd, (struct sockaddr *)&client_addr, &len)) < 0){
            perror("Errore nell'accettazione della connessione");
            close(sd);
            exit(EXIT_FAILURE);
        }
        printf("Connessione accettata da un utente.\n");

        /* Creazione thread per la gestione dell'utente */
        pthread_t td;
        if(pthread_create(&td, NULL, gestione_utente, (void *)(intptr_t)new_socket) != 0){
            perror("Errore nella creazione del thread per l'utente");
            close(new_socket);
            continue;
        }
        pthread_detach(td);
    }

    close(sd);
    return 0;
}