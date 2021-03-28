CFLAGS = -Wall

all: tftp_client tftp_server 
tftp_client: tftp_client.o
	$(CC) $(CFLAGS) tftp_client.o -o tftp_client
tftp_client.o: tftp_client.c
	$(CC) $(CFLAGS) -c tftp_client.c
tftp_server: tftp_server.o
	$(CC) $(CFLAGS) tftp_server.o -o tftp_server
tftp_server.o: tftp_server.c
	$(CC) $(CFLAGS) -c tftp_server.c
clean:
	rm *.o tftp_client tftp_server