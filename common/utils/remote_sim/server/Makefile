# SPDX-License-Identifier: LicenseRef-CSSL-1.0

CC = gcc
CFLAGS = -g -Wall

OBJS = main.o pl2303.o atr.o tpdu-apdu.o usim.o uicc.o util.o net.o log.o

simcard: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f simcard *.o
