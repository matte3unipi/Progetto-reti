gcc -Wall -c lavagna.c 
gcc -Wall -c utente.c

gcc -o lavagna lavagna.o -lpthread
gcc -o utente utente.o -lpthread

echo "Compilazione completata."