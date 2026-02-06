CC=clang
CFLAGS=-O2 -Wall

all: victim attacker

victim: victim.c
	$(CC) $(CFLAGS) victim.c -o victim

attacker: attacker.c
	$(CC) $(CFLAGS) attacker.c -o attacker

clean:
	rm -f victim attacker
