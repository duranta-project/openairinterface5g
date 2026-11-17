# SPDX-License-Identifier: LicenseRef-CSSL-1.0

CC = gcc
CFLAGS = -g -Wall

OBJS = main.o pl2303.o pcsc.o atr.o tpdu-apdu.o usim.o uicc.o util.o net.o log.o

ifneq ($(shell pkg-config --silence-errors --modversion libpcsclite),)
PCSC_ALL_CFLAGS += -DPCSC $(shell pkg-config --cflags libpcsclite)
PCSC_DEF_CFLAGS += -DPCSC
LDFLAGS += $(shell pkg-config --libs libpcsclite)
endif

simcard: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

main.o: main.c
	$(CC) $(CFLAGS) $(PCSC_DEF_CFLAGS) -c -o $@ $<

pcsc.o: pcsc.c
	$(CC) $(CFLAGS) $(PCSC_ALL_CFLAGS) -c -o $@ $<

clean:
	rm -f simcard *.o
