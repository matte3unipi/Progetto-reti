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

const char *comandi_validi[] = {
    "HELLO", "QUIT", "CREATE_CARD", 
    "REQUEST_USER_LIST", "REVIEW_CARD", "CARD_DONE",
    "SHOW_LAVAGNA"
};
const int num_comandi = 7;





/*Funzione per gestire il comando HELLO */
void hello_function(int sd, int porta_utente) {
    char msg[20];
    sprintf(msg, "HELLO:%d", porta_utente);

    int byte_inviati = send(sd, msg, strlen(msg), 0);
    if (byte_inviati < 0) {
        perror("Errore durante l'invio del messaggio HELLO");
        exit(EXIT_FAILURE);
    } else {
        printf("Registrazione in corso.\n");
    }

    char buffer[256];
    int bytes_letti = recv(sd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_letti <= 0) {
        printf("Connessione chiusa dalla lavagna.\n");
    }
    
    buffer[bytes_letti] = '\0';
    printf("Risposta dalla lavagna: %s\n", buffer);

    return;
}




int main(int argc, char *argv[]) {
    int porta_utente = 0;
    
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
    

    /* Controllo che i comandi inseriti da tastiera siano quelli permessi.*/
    char comando[20];

    while (1){
        printf("Inserire comando?\n");
        scanf("%s", comando);

        if (strcmp(comando,"si") == 0)
            system("clear");
        else
            continue;
        
        printf("Inserisci il comando\n");
        scanf("%s", comando);

        int comando_valido = 0;
        for (int i = 0; i < num_comandi; i++) {
            if (strcmp(comando, comandi_validi[i]) == 0) {
                comando_valido = 1;

                // Invia il comando alla lavagna
                if(strcmp(comando, "HELLO") == 0)
                    hello_function(sd, porta_utente);

                break;
            }
        }
        if (!comando_valido) {
            printf("Comando non permesso.\n");
            continue;
        }
    }
    
    close(sd);
    return 0;
}