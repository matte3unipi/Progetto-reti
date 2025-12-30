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
#include "funzioni_x_msg.h"             // Per send_message e recv_message

/*Definizione funzioni per le chiamate*/
int registrazione_utente(int porta_utente, int sd_utente);
int rimozione_utente(int porta_utente);
int controllo_stato(const char *str);
int create_card(const char* comando, int porta_utente);
int move_card(int id_card, STATO_COLONNA vecchia_colonna, STATO_COLONNA nuova_colonna);
int ack_card(const char* buffer, int porta_utente);
int card_done(const char* buffer, int porta_utente);
void card_doing_check(int porta_utente);
int send_user_list(int sd, int porta_destinatario);
void handle_card();
int show_lavagna();
void creazione_lavagna();
int send_show_lavagna(int sd_utente);

/*
* ============================================================================ *
* ============================================================================ *
                                GESTIONE UTENTI
* ============================================================================ *
* ============================================================================ *
*/

/*Funzione per la registrazione di un utente*/
int registrazione_utente(int porta_utente, int sd_utente){
    
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

    /*Salvo il socket dell'utente + la porta per identificare il socket in altre funzioni*/
    id_socket_x_lavagna[lavagna->numero_utenti_connessi - 1].socket_id = sd_utente;
    id_socket_x_lavagna[lavagna->numero_utenti_connessi - 1].occupato = 0;
    id_socket_x_lavagna[lavagna->numero_utenti_connessi - 1].porta_utente = porta_utente;
    id_socket_x_lavagna[lavagna->numero_utenti_connessi - 1].pong_ricevuto = 0;

    return 0;
}


/*Funzione per la rimozione di un utente in seguito alla QUIT*/
int rimozione_utente(int porta_utente){
    int trovato = 0;

    /*Devo controllare che l'utente non abbia card in corso*/
    card_doing_check(porta_utente);
    
    /*Cerco l'utente e lo rimuovo dalla lista*/
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(lavagna->porta_utenti_connessi[i] == porta_utente) {
            trovato = 1;

            /*Rimuovo la porta e il socket associato*/
            id_socket_x_lavagna[i].socket_id = 0;
            id_socket_x_lavagna[i].occupato = 0;
            id_socket_x_lavagna[i].porta_utente = 0;
        }
        if(trovato && i < lavagna->numero_utenti_connessi - 1) {
            lavagna->porta_utenti_connessi[i] = lavagna->porta_utenti_connessi[i + 1];
            id_socket_x_lavagna[i] = id_socket_x_lavagna[i + 1];
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

/*Thread per eseguire il ping ad un utente*/
void* ping_user(void* arg){
    int porta_utente = (int)(intptr_t)arg;
    int socket_utente = -1;

    /* Trovo il socket dell'utente dalla porta */
    pthread_mutex_lock(&accesso_lavagna);
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(id_socket_x_lavagna[i].porta_utente == porta_utente){
            socket_utente = id_socket_x_lavagna[i].socket_id;
            break;
        }
    }
    pthread_mutex_unlock(&accesso_lavagna);

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
    sleep(30);

    /* Controllo se è stato ricevuto il pong */
    pthread_mutex_lock(&accesso_lavagna);
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(id_socket_x_lavagna[i].porta_utente == porta_utente){

            if(id_socket_x_lavagna[i].pong_ricevuto == 0){
                printf("Nessun pong ricevuto dall'utente sulla porta %d. Procedo alla rimozione.\n", porta_utente);
                /* Rimuovo l'utente */
                rimozione_utente(porta_utente);
                close(socket_utente);
            } else {
                /* Resetto il flag per il prossimo ping */
                id_socket_x_lavagna[i].pong_ricevuto = 0;
            }
            break;
        }
    }
    pthread_mutex_unlock(&accesso_lavagna);
    return NULL;
}

/*Thread per il ping degli utenti connessi*/
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

/*Funzione per controllo valore colonna passato*/
int controllo_stato(const char *str) {
    if(strcmp(str, "TO_DO") == 0) 
        return TO_DO;
    if(strcmp(str, "DOING") == 0) 
        return DOING;
    if(strcmp(str, "DONE") == 0) 
        return DONE;
    return -1; 
}

/*Funzione per controllare e spostare le card in TO_DO di un utente che si disconnette*/
void card_doing_check(int porta_utente){
    int colonna = (int)DOING;

    for(int i = 0; i < lavagna->colonne[colonna].numero_card; i++){
        if(lavagna->colonne[colonna].cards[i].porta_utente == porta_utente){
            /* Sposto la card indietro in TO_DO */
            int id_card = lavagna->colonne[colonna].cards[i].id;
            lavagna->colonne[colonna].cards[i].porta_utente = 0;
            lavagna->colonne[colonna].cards[i].timestamp_ultima_modifica = time(NULL);
            move_card(id_card, DOING, TO_DO);
            i--; 
        }
    }
}

/*Funzione per la creazione di una nuova card*/
int create_card(const char* comando, int porta_utente){
    /* Parsing del comando */
    char card_copia[512];
    strncpy(card_copia, comando, sizeof(card_copia));
    card_copia[sizeof(card_copia) - 1] = '\0';

    /* Estraggo i campi della card */
    char *token = strtok(card_copia, ":");
    token = strtok(NULL, ":");
    int id_card = atoi(token);

    token = strtok(NULL, ":");
    STATO_COLONNA colonna = controllo_stato(token);
    if(colonna == -1){
        return -1;
    }
    token = strtok(NULL, ":");
    char *testo = token;

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


/*Funzione per spostare una card da una colonna ad un'altra*/
int move_card(int id_card, STATO_COLONNA vecchia_colonna, STATO_COLONNA nuova_colonna){
    /* Cerco la card nella lavagna */
    int vecchia_col_id = (int)vecchia_colonna;
    int nuova_col_id = (int)nuova_colonna;
    int trovata = 0;
    struct st_CARD card_select;
    int posizione_card = -1;


    for(int i = 0; i < lavagna->colonne[vecchia_col_id].numero_card; i++){
        if(lavagna->colonne[vecchia_col_id].cards[i].id == id_card){
            card_select = lavagna->colonne[vecchia_col_id].cards[i];
            posizione_card = i;
            trovata = 1;
            break;
        }
    }
    if(!trovata){
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
    return 0;
}


/*Funzione per spostare una card da TO_DO a DOING in seguito ad un ack*/
int ack_card(const char* buffer, int porta_utente){
    /*Estraggo l'id e la colonna della card in questione*/
    char ack_copia[64];
    strncpy(ack_copia, buffer, sizeof(ack_copia));
    ack_copia[sizeof(ack_copia) - 1] = '\0';

    char *token = strtok(ack_copia, ":");
    token = strtok(NULL, ":");
    int id_card = atoi(token);


    // Trova la card in TO_DO e salva la porta
    for(int i = 0; i < lavagna->colonne[TO_DO].numero_card; i++){
        if(lavagna->colonne[TO_DO].cards[i].id == id_card){
            lavagna->colonne[TO_DO].cards[i].porta_utente = porta_utente;
            lavagna->colonne[TO_DO].cards[i].timestamp_ultima_modifica = time(NULL);
            break;
        }
    }

    if(move_card(id_card, TO_DO, DOING) != 0){
        return -1;
    }
    return 0;
}

/*Funzione per spostare una card da DOING a DONE in seguito ad un ack*/
int card_done(const char* buffer, int porta_utente){
    /*Estraggo l'id e la colonna della card in questione*/
    char ack_copia[64];
    strncpy(ack_copia, buffer, sizeof(ack_copia));
    ack_copia[sizeof(ack_copia) - 1] = '\0';

    char *token = strtok(ack_copia, ":");
    token = strtok(NULL, ":");
    int id_card = atoi(token);

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

    /*Una volta arrivato l'ack l'utente non è più occupato*/
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(id_socket_x_lavagna[i].porta_utente == porta_utente){
            id_socket_x_lavagna[i].occupato = 0;
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
    strcpy(lista, "SEND_USER_LIST:");


    /*Inserisco numero utenti (meno 1 del destinatario)*/
    char num_utenti_str[10];
    sprintf(num_utenti_str, "%d", lavagna->numero_utenti_connessi - 1);
    strcat(lista, num_utenti_str);
    strcat(lista, ":");

    /*Inserisco lista porte utenti (escludendo la porta del destinatario)*/
    for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
        if(lavagna->porta_utenti_connessi[i] == porta_destinatario){
            continue;
        }
        char porta_str[10];
        sprintf(porta_str, "%d", lavagna->porta_utenti_connessi[i]);
        strcat(lista, porta_str);
        if(i < lavagna->numero_utenti_connessi - 1){
            strcat(lista, ":");
        }
    }

    if(send_message(sd, lista) < 0){
        perror("Errore nell'invio della lista utenti");
        rimozione_utente(porta_destinatario);
        close(sd);
        return -1;
    }

    return 0;
}


int send_show_lavagna(int sd_utente){
    char msg[8192];
    int pos = 0;

    /* Formato: LAVAGNA_STATE:id_lavagna|max_cards|card1_col1:card2_col1:...|card1_col2:card2_col2...| */

    pos += sprintf(msg + pos, "LAVAGNA_STATE:%d|", lavagna->id);
    
    int max_cards = 0;
    for(int i = 0; i < NUM_COLONNE; i++){
        if(lavagna->colonne[i].numero_card > max_cards){
            max_cards = lavagna->colonne[i].numero_card;
        }
    }
    
    pos += sprintf(msg + pos, "%d|", max_cards);
    
    for(int col = 0; col < NUM_COLONNE; col++){
        for(int i = 0; i < max_cards; i++){
            if(i < lavagna->colonne[col].numero_card){
                struct st_CARD card = lavagna->colonne[col].cards[i];
                /* Aggiungo id e testo della card */
                pos += sprintf(msg + pos, "%d,%s", card.id, card.testo);
            }
            if(i < max_cards - 1){
                pos += sprintf(msg + pos, ":");
            }
        }
        if(col < NUM_COLONNE - 1){
            pos += sprintf(msg + pos, "|");
        }
    }

    if(send_message(sd_utente, msg) < 0){
        perror("Errore nell'invio dello stato della lavagna");
        return -1;
    }
    return 0;
}



/*Funzione per l'attribuzione di card agli utenti connessi */
void handle_card(){

    int colonna_to_do = (int)TO_DO;

    for(int i = 0; i < lavagna->colonne[colonna_to_do].numero_card; i++){

        struct st_CARD card_selezionata = lavagna->colonne[colonna_to_do].cards[i];

        /* Assegno la card al primo utente disponibile (occupato = 0) */
        for(int j = 0; j < lavagna->numero_utenti_connessi; j++){

            if(id_socket_x_lavagna[j].occupato == 0){
                /* Costruisco il messaggio con lista utenti (escluso destinatario) */
                char msg[1024];
                char *pos = msg;
                int num_utenti_escl = lavagna->numero_utenti_connessi - 1;
                
                /* Inizio messaggio con ID, testo e numero utenti */
                pos += sprintf(pos, "HANDLE_CARD:%d:%s:%d", 
                    card_selezionata.id, card_selezionata.testo, num_utenti_escl);
                
                /* Aggiungo lista porte (escludendo il destinatario alla posizione j) */
                for(int k = 0; k < lavagna->numero_utenti_connessi; k++){
                    if(k != j){
                        pos += sprintf(pos, ":%d", lavagna->porta_utenti_connessi[k]);
                    }
                }
                
                /* Invio la card all'utente */
                if(send_message(id_socket_x_lavagna[j].socket_id, msg) < 0){
                    perror("Errore nell'invio della card all'utente");
                }

                /* Segno l'utente come occupato */
                id_socket_x_lavagna[j].occupato = 1;
                break;
            }
        }
    }

    printf("Card inviate, attesa ACK da parte degli utenti.\n");

    return;
}


/*
* ============================================================================ *
* ============================================================================ *
                            GESTIONE LAVAGNA
* ============================================================================ *
* ============================================================================ *

*/

/*Funzione per la visualizzazione della lavagna*/
int show_lavagna(){
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

    return 0;
}

/*Funzione per la creazione della lavagna e inizializzazione*/
void creazione_lavagna(){
    /*Inizializzazione semafori*/
    pthread_mutex_init(&accesso_lavagna, NULL);
    pthread_mutex_init(&accesso_lista_socket, NULL);

    /*Creazione lavagna*/
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


/*
* ============================================================================ *
* ============================================================================ *
                    THREAD GESTIONE COMANDI LAVAGNA E UTENTI
* ============================================================================ *
* ============================================================================ *
*/


/*Funzione per la gestione dei comandi da terminale della lavagna*/
void* gestione_lavagna(void* arg){
    char comando[50];

    while(1){
        fgets(comando, sizeof(comando), stdin);
        comando[strcspn(comando, "\n")] = 0; 

        if(strcmp(comando, "SHOW_LAVAGNA") == 0){
            pthread_mutex_lock(&accesso_lavagna);
            show_lavagna();
            pthread_mutex_unlock(&accesso_lavagna);
            continue;
        } 
        
        if(strcmp(comando, "HANDLE_CARD") == 0) {
            pthread_mutex_lock(&accesso_lavagna);
            handle_card();
            pthread_mutex_unlock(&accesso_lavagna);
            continue;
        }
        printf("Comando non riconosciuto.\n");
    }

    return NULL;
}



/*Funzione per la gestione di un utente connesso*/
void* gestione_utente(void* arg){
    int sd_utente = (int)(intptr_t)arg;
    char buffer[1024];
    int porta_utente = 0;

    /*Arrivo comandi da parte dell'utente*/
    while(1){
        /*Ricezione messaggio*/
        if(recv_message(sd_utente, buffer, sizeof(buffer)) < 0) {
            printf("Connessione utente sulla porta %d interrotta.\n", porta_utente);
            rimozione_utente(porta_utente);
            close(sd_utente);
            return NULL;
        }

        printf(RIGA_SEPARATORIA);
        printf("Comando ricevuto dall'utente: %s\n", buffer);

        /*Gestione comandi*/
        /*Comando HELLO*/
        if (strncmp(buffer, "HELLO:", 6) == 0) {
            porta_utente = atoi(&buffer[6]);

            pthread_mutex_lock(&accesso_lavagna);
            if (registrazione_utente(porta_utente, sd_utente) != 0) {
                const char *risposta = "Porta già registrata.";
                send_message(sd_utente, risposta);
            }
            else{
                printf("Utente alla porta %d registrato.\n", porta_utente);
                const char *risposta = "Registrazione avvenuta con successo.";
                send_message(sd_utente, risposta);
            }
            
            pthread_mutex_unlock(&accesso_lavagna);
            continue;   
        }
        
        /*Comando SHOW_LAVAGNA*/
        if (strcmp(buffer, "SHOW_LAVAGNA") == 0) {

            pthread_mutex_lock(&accesso_lavagna);
            if(send_show_lavagna(sd_utente) != 0){
                const char *risposta = "Errore nella visualizzazione della lavagna.";
                send_message(sd_utente, risposta);
            }
            
            pthread_mutex_unlock(&accesso_lavagna);
            continue;
        }

        /*Comando QUIT*/
        if (strncmp(buffer, "QUIT:", 5) == 0) {
            printf("Utente sulla porta %d disconnesso.\n", porta_utente);
            pthread_mutex_lock(&accesso_lavagna);
            rimozione_utente(porta_utente);
            pthread_mutex_unlock(&accesso_lavagna);
            close(sd_utente);
            break;
        }

        /*Comando CREATE_CARD*/
        if (strncmp(buffer, "CREATE_CARD:", 12) == 0){
            printf("Comando CREATE_CARD ricevuto dalla porta %d.\n", porta_utente);
            pthread_mutex_lock(&accesso_lavagna);
            if(create_card(buffer, porta_utente) != 0){
                const char *risposta = "Errore nella creazione della card (ID possibile duplicato).";
                send_message(sd_utente, risposta);
            } 
            else {
                const char *risposta = "Card creata con successo.";
                send_message(sd_utente, risposta);                
            }

            pthread_mutex_unlock(&accesso_lavagna);
            continue;
        }

        /*Comando ACK_CARD*/
        if(strncmp(buffer, "ACK_CARD:", 9) == 0){
            printf("Comando ACK_CARD ricevuto dalla porta %d.\n", porta_utente);
            pthread_mutex_lock(&accesso_lavagna);
            if(ack_card(buffer, porta_utente) != 0){
                const char *risposta = "Errore nell'ack della card.";
                send_message(sd_utente, risposta);
            }
            else {
                const char *risposta = "Card acked con successo.";
                send_message(sd_utente, risposta);                
            }

            pthread_mutex_unlock(&accesso_lavagna);
            continue;
        }

        /*Comando REQUEST_USER_LIST*/
        if (strcmp(buffer, "REQUEST_USER_LIST") == 0) {
            pthread_mutex_lock(&accesso_lavagna);
            if(send_user_list(sd_utente, porta_utente) != 0){
                const char *risposta = "Errore nell'invio della lista utenti.";
                send_message(sd_utente, risposta);
            }
            else {
                printf("Lista utenti inviata con successo alla porta %d.\n", porta_utente);
            }

            pthread_mutex_unlock(&accesso_lavagna);
            continue;
        }

        /*Comando CARD_DONE*/
        if(strncmp(buffer, "CARD_DONE:",10) == 0){
            printf("Comando CARD_DONE ricevuto dalla porta %d.\n", porta_utente);
            pthread_mutex_lock(&accesso_lavagna);
            if(card_done(buffer, porta_utente) != 0){
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
            continue;
        }


        /*Comando PONG + gestione*/
        if(strncmp(buffer, "PONG", 4) == 0){
            pthread_mutex_lock(&accesso_lista_socket);
            for(int i = 0; i < lavagna->numero_utenti_connessi; i++){
                if(id_socket_x_lavagna[i].porta_utente == porta_utente){
                    id_socket_x_lavagna[i].pong_ricevuto = 1;
                    printf("Pong ricevuto dall'utente sulla porta %d.\n", porta_utente);
                    break;
                }
            }
            pthread_mutex_unlock(&accesso_lista_socket);
            continue;
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
        perror("Errore nel binding della socket");
        close(sd);
        exit(EXIT_FAILURE);
    }
    /* Messa in ascolto della socket */
    listen(sd, MIN_UTENTI);

    /*Creazione thread ascolto comandi lavagna*/
    pthread_t thread_lavagna;
    pthread_create(&thread_lavagna, NULL, gestione_lavagna, NULL);
    pthread_detach(thread_lavagna);

    /*Creazione thread per PING_USER*/
    pthread_t thread_ping;
    pthread_create(&thread_ping, NULL, handler_ping_users, NULL);
    pthread_detach(thread_ping);

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

        pthread_t td;
        pthread_create(&td, NULL, gestione_utente, (void *)(intptr_t)new_socket);
        pthread_detach(td);
    }

    close(sd);
    return 0;
}