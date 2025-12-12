#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define PORTA_LAVAGNA 5678

int main(int argc, char *argv[]) {
    int porta_utente = 0;
    const char *comandi_validi[] = {
        "HELLO", "QUIT", "CREATE_CARD", 
        "REQUEST_USER_LIST", "REVIEW_CARD", "CARD_DONE",
        "SHOW_LAVAGNA"
    };
    int num_comandi = 7;
    
    if (argc != 2) {
        printf("L'eseguibile richiedela porta.\n");
        return -1;
    }
    
    porta_utente = atoi(argv[1]);

    /*
    * implementazione della connessione.
    */

    /* Controllo che i comandi inseriti da tastiera siano quelli permessi.*/
    char comando[20];

    while (1){
        printf("Inserisci comando:\n");
        scanf("%s", comando);

        int comando_valido = 0;
        for (int i = 0; i < num_comandi; i++) {
            if (strcmp(comando, comandi_validi[i]) == 0) {
                comando_valido = 1;
                break;
            }
        }
        if (!comando_valido) {
            printf("Comando non permesso.\n");
            continue;
        }
    }
    
    return 0;
}