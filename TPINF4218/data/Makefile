CC=g++
CFLAGS=-Wall -pthread -Wextra -std=c++17

all: serveur client

client: client.o 
	$(CC) $(CFLAGS) $^ -o $@
	
serveur: serveur.o 
	$(CC) $(CFLAGS) $^ -o $@	

client.o: client.cpp 
	$(CC) $(CFLAGS) -c $< -o $@

serveur.o: serveur.cpp 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o client serveur

