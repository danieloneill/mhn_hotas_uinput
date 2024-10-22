all: hori

hori: hori.o
	$(CC) -o hori hori.c -lusb-1.0
