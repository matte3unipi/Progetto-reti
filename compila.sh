gcc -Wall -c lavagna.c 
gcc -Wall -c utente.c 
gcc -Wall -c funzioni_x_msg.c

gcc -o lavagna lavagna.o funzioni_x_msg.o -lpthread
gcc -o utente utente.o funzioni_x_msg.o -lpthread

echo "Compilazione completata."