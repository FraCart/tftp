#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <stdlib.h>

// macro
#define MAX_BUF_LEN 1024
#define MAX_BLOCK_LEN 512

#define BINARY_MODE "binario\0"
#define LENGHT 800

#define TEXT_MODE "testuale\0"
#define MAX_COMMAND_LEN 20

// var globali
int s, ret, len;
char *tipo_trasferimento = BINARY_MODE;
char nome_file[MAX_BUF_LEN];
char nome_locale[MAX_BLOCK_LEN];
char *err_msg;
char *data;
FILE *fp;
struct sockaddr_in server_addr, my_addr;
struct sockaddr_storage destination_port;

// totale byte ricevuti
size_t totale_ricevuti = 0;

// totale byte ricevuti nella singola transazione
size_t bytes_ricevuti;

// identificatori per la struttura dei messaggi:
//       opcode del       opcode del
//       messaggio di     messagio
//       risposta         da inviare
uint16_t opcode_risposta, opcode_send, block, tipo_messaggio;

char buffer[MAX_BUF_LEN];

// Funzione di utilita', riceve l'identificatore del blocco ricevuto
// e manda il messaggio di ACK al server
void ack(uint16_t block_number)
{
  char *pacchetto_di_ack;
  uint16_t codice_di_ack = htons(4);
  pacchetto_di_ack = malloc(4);
  memcpy(pacchetto_di_ack, &codice_di_ack, 2);
  memcpy((pacchetto_di_ack + 2), &block_number, 2);
  ret = sendto(s, pacchetto_di_ack, 4, 0, (struct sockaddr *)&destination_port, sizeof(destination_port));
  if (ret < 0)
  {
    printf("[X] Si è verificato un errore durante l'invio dell'ACK\n\n");
    exit(1);
  }
  free(pacchetto_di_ack);
}

// Funzione di utilita', riceve i byte dal buffer (var. globale)
// conservandoli in 'data' e 'block' e restituisce quanti byte sono stati ricevuti
// nella singola transazione
size_t ricevi_bytes()
{
  // salvo il block scorrendo di due (saltando la parte OPCODE)
  memcpy(&block, (buffer + 2), 2);
  block = ntohs(block);

  bytes_ricevuti = len - 4;
  data = (char *)realloc((void *)data, (totale_ricevuti + bytes_ricevuti));
  memcpy((data + totale_ricevuti), (buffer + 4), bytes_ricevuti);

  ack(htons(block));

  totale_ricevuti += bytes_ricevuti;
  return bytes_ricevuti;
}

// Scrive 'data' (var. globale) su file
void write_and_close()
{
  int i;
  for (i = 0; i < totale_ricevuti; i++)
  {
    fwrite((data + i), 1, 1, fp);
  }
  totale_ricevuti = 0;
  free(data);
  fclose(fp);
  printf("[!] Il salvataggio del file %s e' stato completato\n", nome_locale);
}

int main(int argc, char *argv[])
{
  char *packet;
  int len_rrq;
  size_t message_size;
  char cmd[MAX_COMMAND_LEN];
  char nome_file[MAX_BLOCK_LEN];

  if (argc != 3)
  {
    printf("[!] Per avviare il client correttamente digita ./tftp_client <IP> <porta>\n");
    return 0;
  }

  printf("[*] Connessione in corso...\n");
  // init socket UDP
  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s == -1)
  {
    printf("[X] Errore nella creazione del socket\n");
    exit(1);
  }
  else
  {
    printf("[*] Socket avviato...\n");
  }
  // inizializzo le strutture per gli indirizzi
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(argv[2]));
  inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = 0;
  my_addr.sin_addr.s_addr = INADDR_ANY;

  // bind al server
  ret = bind(s, (struct sockaddr *)&my_addr, sizeof(my_addr));
  if (ret < 0)
  {
    printf("[X] Errore in fase di bind\n");
    return 0;
  }

  printf("[!] Connessione stabilita con %s\n", argv[1]);

  printf("\n[*] I comandi disponibili sono:\n");
  printf("!help: mostra l'elenco dei comandi\n");
  printf("!mode <txt|bin>: imposta la modalita' di trasferimento (testo/binario)\n");
  printf("!get filename nome_locale: richiede al server il nome del file <filename> e lo salva con il nome <nome_locale>\n");
  printf("!quit: termina il client\n");

  while (1)
  {
    printf("\n%s", "> ");

    fgets(buffer, MAX_BUF_LEN, stdin);
    sscanf(buffer, "%s %s %s", cmd, nome_file, nome_locale);

    if (strcmp(buffer, "!help\n") == 0)
    {
      printf("!help: mostra l'elenco dei comandi\n");
      printf("!mode <txt|bin>: imposta la modalita' di trasferimento (testo/binario)\n");
      printf("!get filename nome_locale: richiede al server il nome del file <filename> e lo salva con il nome <nome_locale>\n");
      printf("!quit: termina il client\n");
    }
    else if (strcmp(buffer, "!mode bin\n") == 0)
    {
      tipo_trasferimento = BINARY_MODE;
      printf("[*] Hai impostato il trasferimento binario\n");
    }
    else if (strcmp(buffer, "!quit\n") == 0)
    {
      exit(1);
    }
    else if (strcmp(buffer, "!mode txt\n") == 0)
    {
      tipo_trasferimento = TEXT_MODE;
      printf("[*] Hai impostato il trasferimento testuale\n");
    }
    else if (strncmp(buffer, "!get", 4) == 0)
    {
      printf("[*] Richiesta file '%s' in corso...\n", nome_file);

      // costruzione del messaggio RRQ
      len_rrq = 0;
      message_size = 6 + strlen(nome_file) + strlen(tipo_trasferimento);
      packet = malloc(message_size); // alloco preventivamente il messaggio in memoria

      // opcode_risposta
      opcode_send = htons(1);
      memcpy(packet, &opcode_send, 2); // opcode_risposta che occupa 2 byte
      len_rrq = len_rrq + 2;

      // nome file
      strcpy((packet + len_rrq), nome_file);
      len_rrq = len_rrq + strlen(nome_file) + 1;

      // byte 0x00
      memset((packet + len_rrq), 0, sizeof(char));
      len_rrq++;

      // tipo trasferimento
      strcpy((packet + len_rrq), tipo_trasferimento);
      len_rrq = len_rrq + strlen(tipo_trasferimento) + 1;
      printf("[*] Modalità di trasferimento: '%s'\n", tipo_trasferimento);

      // byte 0x00
      memset((packet + len_rrq), 0, sizeof(char));

      ret = sendto(s, packet, message_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
      if (ret < 0)
      {
        printf("[X] Invio richiesta non riuscito");
        exit(1);
      }
      free(packet);

      do
      {
        // Gestione risposta
        int addrlen = sizeof(destination_port);
        len = recvfrom(s, buffer, MAX_BUF_LEN, 0, (struct sockaddr *)&destination_port, (socklen_t *)&addrlen);
        if (len == -1)
        {
          printf("[X] Errore durante la ricezione\n");
          break;
        }
        int posizione = 0;

        // gestione opcode della risposta
        memcpy(&tipo_messaggio, buffer + posizione, sizeof(tipo_messaggio));
        posizione += sizeof(tipo_messaggio);
        opcode_risposta = ntohs(tipo_messaggio);

        if (opcode_risposta != 3 && opcode_risposta != 5)
        {
          printf("[X] Operazione non consentita\n");
          break;
        }

        if (opcode_risposta == 5)
        {
          err_msg = buffer + 4;
          printf("[X] %s\n", err_msg);
          break;
        }

        if (opcode_risposta == 3)
        {
          bytes_ricevuti = ricevi_bytes();

          if (bytes_ricevuti < MAX_BLOCK_LEN)
          {
            printf("[*] Trasferimento completato (%u/%u blocchi)\n", block, block);
          }
        }
      } while (len > MAX_BLOCK_LEN);

      if (opcode_risposta == 3)
      {
        // Scrivo il file ricevuto
        if (!strcmp(tipo_trasferimento, BINARY_MODE))
        {
          fp = fopen(nome_locale, "a+");
        }
        else if (!strcmp(tipo_trasferimento, TEXT_MODE))
        {
          fp = fopen(nome_locale, "ab+");
        }
        if (fp == NULL)
        {
          printf("[X] Errore durante il salvataggio del file\n");
          exit(1);
        }
        write_and_close();
      }
    }
    else
    {
      printf("[X] Comando non trovato, digita !help per la lista completa dei comandi\n");
    }
  }
  close(s);
  exit(0);
}