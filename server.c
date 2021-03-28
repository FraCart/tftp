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
int new_sock;
int pos;

unsigned int address_length;
unsigned int remained;
pid_t PID;
char buffer[BUF];
char nome_file[BUF];
char mode[BUF];
char pack[BUF];
//buffer di errore
char Err_buff[BUF];

FILE *fp;
uint16_t block_succ;
unsigned char buffer_bin[SIZE_FILE];
char message[SIZE_FILE];
char buffer_file[SIZE_FILE];

struct sockaddr_in my_addr, client_addr, new_addr;

int Binary_mode_sending(uint16_t block_num, char *pack, unsigned char *buffer_bin, FILE *fp, unsigned int dim_pkt)
{

  memset(pack, 0, BUF);

  uint16_t opcode = htons(3);

  int posizione = 0;

  memset(buffer_bin, 0, SIZE_FILE);

  fread(buffer_bin, dim_pkt, 1, fp);

  memcpy(pack + posizione, (uint16_t *)&opcode, 2);
  posizione += 2;
  memcpy(pack + posizione, (uint16_t *)&block_num, 2);
  posizione += 2;
  memcpy(pack + posizione, buffer_bin, dim_pkt);
  posizione += dim_pkt;
  return posizione;
}

int Text_mode_sending(uint16_t block_num, char *pack, char *buffer_file, FILE *fp, unsigned int dim_pkt)
{

  memset(pack, 0, BUF);
  uint16_t opcode = htons(3);
  int posizione = 0;
  memset(buffer_file, 0, SIZE_FILE);

  fread(buffer_file, dim_pkt, 1, fp);

  memcpy(pack + posizione, (uint16_t *)&opcode, 2);
  posizione += 2;
  memcpy(pack + posizione, (uint16_t *)&block_num, 2);
  posizione += 2;
  strcpy(pack + posizione, buffer_file);
  posizione += dim_pkt;
  return posizione;
}

void SetParameters(int port)
{
  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(port);
  my_addr.sin_addr.s_addr = INADDR_ANY;
  printf("\nSocket creato.\n");
}

int Error(uint16_t errorCode, char *Err_buff, char *file_name, char *message)
{

  int posizione = 0;

  memset(Err_buff, 0, BUF);

  uint16_t opcode = htons(5);

  memcpy(Err_buff, (uint16_t *)&opcode, 2);
  posizione += 2;
  memcpy(Err_buff + posizione, (uint16_t *)&errorCode, 2);
  posizione += 2;
  //copio messaggio
  strcpy(Err_buff + posizione, message);
  posizione += strlen(message) + 1;

  return posizione;
}

int OpenConnection()
{
  int new_sock = socket(AF_INET, SOCK_DGRAM, 0);
  memset(&new_addr, 0, sizeof(new_addr));
  new_addr.sin_family = AF_INET;
  new_addr.sin_port = htons(0);
  new_addr.sin_addr.s_addr = INADDR_ANY;
  return new_sock;
}

int main(int argc, char **argv)
{

  if (argc != 3)
  {
    printf("\nPer avviare il programma digita ./tftp_server porta directory files\n");
    return 0;
  }

  int port = atoi(argv[1]);
  //la directory che viene passata
  char *dir = argv[2];

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
  {
    printf("Si è verificato un errore in fase di connessione: \n");
    exit(0);
  }

  //creo indirizzo di bind passando i parametri
  SetParameters(port);
  ret = bind(sock, (struct sockaddr *)&my_addr, sizeof(my_addr));

  if (ret < 0)
  {
    printf("Si è verificato un errore in fase di bind: \n");
    close(ret);
    exit(0);
  }
  for (;;)
  {

    memset(&buffer, 0, BUF);
    address_length = sizeof(client_addr);
    //accetto nuove connessioni
    ret = recvfrom(sock, (void *)buffer, REQ, 0, (struct sockaddr *)&client_addr, &address_length);

    if (ret < 0)
    {
      printf("Si è verificato un errore in fase di ricezione: \n");
      exit(0);
    }
    else
    {
      printf("\nRicevuto ...\n");
    }

    PID = fork();
    if (PID == -1)
    {

      printf("Fork Error\n");
      exit(-1);
    }
    if (PID == 0)
    {
      //sono il processo figlio
      //qui apro la connessione per inviare passo i parametri a una funzione esterna
      new_sock = OpenConnection();

      ret = bind(new_sock, (struct sockaddr *)&new_addr, sizeof(new_addr));

      if (ret < 0)
      {
        printf("Si è verificato un errore in fase di bind: \n");
        exit(0);
      }
      close(sock);

      uint16_t opcode, errorCode;
      memcpy(&opcode, buffer, 2);
      ok = 0;
      opcode = ntohs(opcode);

      if (opcode == 1)
      {
        //qui l'opcode è valido
        memset(nome_file, 0, BUF);
        strcpy(nome_file, buffer + 2);
        strcpy(mode, buffer + (int)strlen(nome_file) + 3);

        printf("\nRichiesto il  download del file %s", nome_file);

        //mi occupo del percorso
        char *path = malloc(strlen(dir) + strlen(nome_file) + 2);
        strcpy(path, dir);
        strcat(path, "/");
        strcat(path, nome_file);

        if (!strcmp(mode, "netascii\0"))
          fp = fopen(path, "r");
        else
          fp = fopen(path, "rb");
        free(path);

        if (fp == NULL)
        {
          errorCode = htons(1);
          memset(message, 0, SIZE_FILE);

          strcpy(message, "File not found\0");
          pos = Error(errorCode, Err_buff, nome_file, message);
          new_sock = socket(AF_INET, SOCK_DGRAM, 0);

          printf("\nATTENZIONE! Lettura del file %s non riuscita\n", nome_file);
          ret = sendto(new_sock, Err_buff, pos, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
          if (ret < 0)
          {
            printf("\nATTENZIONE: Invio dei dati fallito\n");
            exit(0);
          }

          close(new_sock);
          continue;
        }
        else
        {

          printf("\nLettura del file %s riuscita\n", nome_file);

          if (!strcmp(mode, "netascii\0"))
          {

            unsigned int len = 0;
            while (fgetc(fp) != EOF)
              len++;

            fseek(fp, 0, SEEK_SET);

            uint16_t block_num = htons(1);
            unsigned int dim_pkt = (len > SIZE_FILE) ? SIZE_FILE : len;
            remained = len - dim_pkt;
            int pos = Text_mode_sending(block_num, pack, buffer_file, fp, dim_pkt);
            block_succ = 1;

            ret = sendto(new_sock, pack, pos, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));

            if (ret < 0)
            {
              printf("ATTENZIONE: Si è verificato un errore durante l'invio del blocco al client.");
              exit(0);
            }
          }
          else
          {

            fseek(fp, 0, SEEK_END);
            unsigned int len = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            uint16_t block_num = htons(1);
            unsigned int dim_pkt = (len > SIZE_FILE) ? SIZE_FILE : len;
            remained = len - dim_pkt;

            int pos = Binary_mode_sending(block_num, pack, buffer_bin, fp, dim_pkt);
            block_succ = 1;

            ret = sendto(new_sock, pack, pos, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));

            if (ret < 0)
            {
              printf("Errore nella send");
              exit(0);
            }
          }
        }
        ok = 1;
      }
      if (opcode != 1 || (opcode == 4 && ok == 0))
      {
        errorCode = htons(2);
        memset(message, 0, SIZE_FILE);
        strcpy(message, "l'operazione TFTP non è esistente\0");
        pos = Error(errorCode, Err_buff, nome_file, message);
        new_sock = socket(AF_INET, SOCK_DGRAM, 0);

        ret = sendto(new_sock, Err_buff, pos, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));

        if (ret < 0)
        {
          printf("\nATTENZIONE: Si è verificato un errore nell'invio dei dati \n");
          exit(0);
        }

        close(new_sock);
        continue;
      }
      for (;;)
      {
        memset(pack, 0, BUF);
        address_length = sizeof(client_addr);

        ret = recvfrom(new_sock, pack, ACK, 0, (struct sockaddr *)&client_addr, &address_length);

        if (ret < 0)
        {
          printf("ATTENZIONE: si è verificato un errore nella receive.\n");
          exit(0);
        }

        uint16_t opcode;
        memcpy(&opcode, pack, 2);

        opcode = ntohs(opcode);

        if (opcode == 4)
        {

          if (remained > 0)
          {

            unsigned int dim_pkt = (remained > SIZE_FILE) ? SIZE_FILE : remained;
            block_succ++;
            remained -= dim_pkt;
            uint16_t block_num = htons(block_succ);

            if (!strcmp(mode, "netascii\0"))
            {
              //modalità text
              pos = Text_mode_sending(block_num, pack, buffer_file, fp, dim_pkt);
              ret = sendto(new_sock, pack, pos, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
            }
            else
            {
              //modalità binaria
              pos = Binary_mode_sending(block_num, pack, buffer_bin, fp, dim_pkt);
              ret = sendto(new_sock, pack, pos, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
            }
          }
          else
          {
            printf("\nTrasferito!");
            //resetto ok
            ok = 0;
            exit(1);
            break;
          }
        }
      }
    }
    if (PID > 0)
    {
      continue;
    }
  }
}
