#define PORTA_LAVAGNA 5678
#define MIN_CARD 10 
#define NUM_COLONNE 3
#define MIN_UTENTI 4

typedef enum {
    TO_DO = 0,
    DOING = 1,
    DONE = 2
} STATO_COLONNA;

struct CARD {
    int id;
    STATO_COLONNA colonna;
    char testo[256];
    int porta_utente;
    time_t timestamp_ultima_modifica;
};

struct COLONNA {
    STATO_COLONNA stato;
    int numero_card;
    struct CARD *cards;
};


struct LAVAGNA {
    int id;
    struct COLONNA colonne[NUM_COLONNE];
    int *porta_utenti_connessi;
    int numero_utenti_connessi;
    int numero_card_totali;
};
