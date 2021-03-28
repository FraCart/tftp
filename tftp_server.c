#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define BUF 1024
#define ACK 4
#define SIZE_FILE 512
#define REQ 516
int ret;
int sock;
int ok;
int new_sd;
int pos;

/***********************************|  
|      opcode  operation            |
|      1     Read request (RRQ)     |
|      2     Write request (WRQ)    |
|      3     Data (DATA)            |
|      4     Acknowledgment (ACK)   |
|      5     Error (ERROR)          |
|***********************************/


pid_t pid;

char pacchetto[BUF];
char err_buffer[BUF];
char buffer[BUF];
char nome_file[BUF];
char mode[BUF];

char message[SIZE_FILE];
char buffer_file[SIZE_FILE];
unsigned char buffer_bin[SIZE_FILE];

unsigned int addrlen;
unsigned int rimanenti;

FILE *fp;
uint16_t block_succ;

struct sockaddr_in my_addr, client_addr, new_addr;

// Costruzione messaggio bin
int binary_builder(uint16_t block_number, char *pacchetto, unsigned char *buffer_bin, FILE *fp, unsigned int dimensione_pkt)
{
  memset(pacchetto, 0, BUF);
  uint16_t opcode = htons(3);
  int posizione = 0;
  memset(buffer_bin, 0, SIZE_FILE);

  fread(buffer_bin, dimensione_pkt, 1, fp);

  memcpy(pacchetto + posizione, (uint16_t *)&opcode, 2);
  posizione += 2;
  memcpy(pacchetto + posizione, (uint16_t *)&block_number, 2);
  posizione += 2;
  memcpy(pacchetto + posizione, buffer_bin, dimensione_pkt);
  posizione += dimensione_pkt;
  return posizione;
}

// Costruzione messaggio txt
int text_builder(uint16_t block_number, char *pacchetto, char *buffer_file, FILE *fp, unsigned int dimensione_pkt)
{
  memset(pacchetto, 0, BUF);
  uint16_t opcode = htons(3);
  int posizione = 0;
  memset(buffer_file, 0, SIZE_FILE);

  fread(buffer_file, dimensione_pkt, 1, fp);

  memcpy(pacchetto + posizione, (uint16_t *)&opcode, 2);
  posizione += 2;
  memcpy(pacchetto + posizione, (uint16_t *)&block_number, 2);
  posizione += 2;
  strcpy(pacchetto + posizione, buffer_file);
  posizione += dimensione_pkt;
  return posizione;
}

// Costruzione messaggio di errore
int error_builder(uint16_t codice_err, char *err_buffer, char *file_name, char *message)
{
  int posizione = 0;
  memset(err_buffer, 0, BUF);
  uint16_t opcode = htons(5);

  memcpy(err_buffer, (uint16_t *)&opcode, 2);
  posizione += 2;
  memcpy(err_buffer + posizione, (uint16_t *)&codice_err, 2);
  posizione += 2;
  strcpy(err_buffer + posizione, message);
  posizione += strlen(message) + 1;

  return posizione;
}

int main(int argc, char **argv)
{

  if (argc != 3)
  {
    printf("[X] Per avviare il programma digita ./tftp_server <porta> <directory files>\n");
    return 0;
  }

  int port = atoi(argv[1]);
  char *dir = argv[2];

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
  {
    printf("[X] Errore creazione socket\n");
    exit(0);
  }

  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(port);
  my_addr.sin_addr.s_addr = INADDR_ANY;
  printf("[*] Indirizzo server inizializzato\n");

  ret = bind(sock, (struct sockaddr *)&my_addr, sizeof(my_addr));
  if (ret < 0)
  {
    printf("[X] Errore bind del socket\n");
    exit(0);
  }
  printf("[!] Listener avviato...\n");

  while (1)
  {
    memset(&buffer, 0, BUF);
    addrlen = sizeof(client_addr);
    // Ricezione connessioni
    ret = recvfrom(sock, (void *)buffer, REQ, 0, (struct sockaddr *)&client_addr, &addrlen);
    if (ret < 0)
    {
      printf("[X] Errore in ricezione\n");
      exit(0);
    }

    pid = fork();
    if (pid == -1)
    {
      printf("[X] Errore nella fork\n");
      exit(-1);
    }

    if (pid == 0)
    {
      // processo figlio per l'invio dei messaggi
      new_sd = socket(AF_INET, SOCK_DGRAM, 0);
      memset(&new_addr, 0, sizeof(new_addr));
      new_addr.sin_family = AF_INET;
      new_addr.sin_port = htons(0);
      new_addr.sin_addr.s_addr = INADDR_ANY;

      ret = bind(new_sd, (struct sockaddr *)&new_addr, sizeof(new_addr));
      if (ret < 0)
      {
        printf("[X] Errore nella fase di bind\n");
        exit(0);
      }

      close(sock);

      uint16_t opcode, codice_err;
      memcpy(&opcode, buffer, 2);
      ok = 0;
      opcode = ntohs(opcode);

      if (opcode == 1)
      {
        memset(nome_file, 0, BUF);
        strcpy(nome_file, buffer + 2);
        strcpy(mode, buffer + (int)strlen(nome_file) + 3);

        printf("[*] Richiesto il  download del file '%s'...", nome_file);

        // creazione path del file
        char *path = malloc(strlen(dir) + strlen(nome_file) + 2);
        strcpy(path, dir);
        strcat(path, "/");
        strcat(path, nome_file);

        if (!strcmp(mode, "netascii\0"))
          fp = fopen(path, "r");
        else
          fp = fopen(path, "rb");
        free(path);

        // Gesione file non trovato
        if (fp == NULL)
        {
          codice_err = htons(1);
          memset(message, 0, SIZE_FILE);

          strcpy(message, "File non trovato\0");
          pos = error_builder(codice_err, err_buffer, nome_file, message);
          new_sd = socket(AF_INET, SOCK_DGRAM, 0);

          printf("\n[X] Lettura del file '%s' non riuscita\n", nome_file);
          ret = sendto(new_sd, err_buffer, pos, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
          if (ret < 0)
          {
            printf("\n[X] Errore invio risposta\n");
            exit(0);
          }

          close(new_sd);
          continue;
        }
        else
        {
          printf("\n[*] File %s trovato\n", nome_file);

          // Invio del PRIMO blocco txt
          if (!strcmp(mode, "netascii\0"))
          {
            unsigned int len = 0;
            while (fgetc(fp) != EOF)
              len++;

            fseek(fp, 0, SEEK_SET);

            uint16_t block_number = htons(1);
            unsigned int dimensione_pkt = (len > SIZE_FILE) ? SIZE_FILE : len;
            rimanenti = len - dimensione_pkt;
            int pos = text_builder(block_number, pacchetto, buffer_file, fp, dimensione_pkt);
            block_succ = 1;

            ret = sendto(new_sd, pacchetto, pos, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
            if (ret < 0)
            {
              printf("[X] Errore trasferimento blocco\n");
              exit(0);
            }
          }
          else // Invio del PRIMO blocco bin
          {
            fseek(fp, 0, SEEK_END);
            unsigned int len = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            uint16_t block_number = htons(1);
            unsigned int dimensione_pkt = (len > SIZE_FILE) ? SIZE_FILE : len;
            rimanenti = len - dimensione_pkt;

            int pos = binary_builder(block_number, pacchetto, buffer_bin, fp, dimensione_pkt);
            block_succ = 1;

            ret = sendto(new_sd, pacchetto, pos, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
            if (ret < 0)
            {
              printf("[X] Errore trasferimento blocco\n");
              exit(0);
            }
          }
        }
        ok = 1;
      }

      // Gestione opcode sconosciuto o non supportato
      if (opcode != 1 || (opcode == 4 && ok == 0))
      {
        codice_err = htons(2);
        memset(message, 0, SIZE_FILE);
        strcpy(message, "operazione TFTP non ammessa\0");
        pos = error_builder(codice_err, err_buffer, nome_file, message);
        new_sd = socket(AF_INET, SOCK_DGRAM, 0);

        ret = sendto(new_sd, err_buffer, pos, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
        if (ret < 0)
        {
          printf("[X] Errore invio risposta\n");
          exit(0);
        }

        close(new_sd);
        continue;
      }

      // Gestione ricezione ACK
      while (1)
      {
        memset(pacchetto, 0, BUF);
        addrlen = sizeof(client_addr);

        ret = recvfrom(new_sd, pacchetto, ACK, 0, (struct sockaddr *)&client_addr, &addrlen);
        if (ret < 0)
        {
          printf("[X] Errore ricezione ack\n");
          exit(0);
        }

        uint16_t opcode;
        memcpy(&opcode, pacchetto, 2);
        opcode = ntohs(opcode);

        if (opcode == 4)
        {
          // Invio blocchi rimanenti
          if (rimanenti > 0)
          {
            unsigned int dimensione_pkt = (rimanenti > SIZE_FILE) ? SIZE_FILE : rimanenti;
            block_succ++;
            rimanenti -= dimensione_pkt;
            uint16_t block_number = htons(block_succ);

            if (!strcmp(mode, "netascii\0"))
            {
              // mode txt
              pos = text_builder(block_number, pacchetto, buffer_file, fp, dimensione_pkt);
              ret = sendto(new_sd, pacchetto, pos, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
            }
            else
            {
              // mode bin
              pos = binary_builder(block_number, pacchetto, buffer_bin, fp, dimensione_pkt);
              ret = sendto(new_sd, pacchetto, pos, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
            }
          }
          else
          {
            printf("[!] File trasferito!\n");
            // reset ok
            ok = 0;
            exit(1);
            break;
          }
        }
      }
    }
    if (pid > 0)
    {
      continue;
    }
  }
}
