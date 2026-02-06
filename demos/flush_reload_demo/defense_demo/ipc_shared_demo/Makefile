CC := gcc
CFLAGS := -Wall -Wextra -O2
LDFLAGS := 

all: producer consumer

producer: producer.c shm_layout.h
	$(CC) $(CFLAGS) -I. -I.. -I../.. -o $@ $<

consumer: consumer.c shm_layout.h
	$(CC) $(CFLAGS) -I. -I.. -I../../defense_demo/ipc_shared_demo -o $@ $<

clean:
	rm -f producer consumer
