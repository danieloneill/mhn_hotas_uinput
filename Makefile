all: hori

hori: hori.c
	$(CC) -o hori hori.c -lusb-1.0 -Ofast

clean:
	rm hori
