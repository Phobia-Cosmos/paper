CC := gcc
CFLAGS := -Wall -Wextra -O2
LDFLAGS := -lpthread

all: demo_sidechannel demo_sidechannel_safe

demo_sidechannel: demo_sidechannel.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

demo_sidechannel_safe: demo_sidechannel_safe.c
	$(CC) $(CFLAGS) -D__STDC_FORMAT_MACROS -D_GNU_SOURCE -D_BSD_SOURCE -DVERSION=1 -o $@ $< $(LDFLAGS)

clean:
	rm -f demo_sidechannel
