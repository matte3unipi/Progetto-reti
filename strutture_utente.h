/*Definizioni*/
#define PORTA_LAVAGNA 5678
#define RIGA_SEPARATORIA "<----------------------------------------\n"
#define CARATTERE_SEPARATORE "|"

/*Dati utili per il codice*/
volatile int hello_eseguito = 0;
int porta_utente = 0;
volatile int ping_ricevuto = 0;
volatile int connessione_attiva = 0;

struct CARD_ASSEGNATA {
    int id;
    char testo[256];
    int* porte_utenti;
    int num_utenti;
    int review_ricevute;
    int card_done_inviata;
};
struct CARD_ASSEGNATA card_assegnata;

pthread_mutex_t ascolto;