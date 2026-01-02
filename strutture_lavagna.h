#define PORTA_LAVAGNA 5678
#define MIN_CARD 10 
#define NUM_COLONNE 3
#define MIN_UTENTI 4
#define RIGA_SEPARATORIA "----------------------------------------\n"

#define MAX_CLIENTS 100

/*Strutture*/
typedef enum {
    TO_DO = 0,
    DOING = 1,
    DONE = 2
} STATO_COLONNA;

struct st_CARD {
    int id;
    STATO_COLONNA colonna;
    char testo[256];
    int porta_utente;
    time_t timestamp_ultima_modifica;
};

struct st_COLONNA {
    STATO_COLONNA stato;
    int numero_card;
    struct st_CARD *cards;
};

struct st_LAVAGNA {
    int id;
    struct st_COLONNA colonne[NUM_COLONNE];
    struct INFO_UTENTE *utenti_connessi;
    int numero_utenti_connessi;
    int numero_card_totali;
};

struct INFO_UTENTE {
    int porta_utente;
    int socket_id;
    int occupato;
    int pong_ricevuto;
};

struct st_LAVAGNA* lavagna = NULL;

/*Semafori*/
pthread_mutex_t accesso_lavagna;
pthread_mutex_t accesso_lista_socket;