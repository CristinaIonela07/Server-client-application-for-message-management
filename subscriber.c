#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "helpers.h"

/*
 * Functie pentru calcularea puterii lui 10 necesara
 * pentru tipul de payload "numar real"
 */
int power(int a) {
  int p = 1;
  while (a != 0) {
    p = p * 10;
    a--;
   }
  return p;
}

/*
 * Functie pentru afisarea mesajului corespunzator in client
 */
void show_message(struct server_mess* message) {
  printf("%s:%d - %s - %s - ", 
          message->ip, message->port, message->topic, message->identifier);

  DIE(message->data_type < 0, "Invalid data type");
  DIE(message->data_type > 3, "Invalid data type");

  uint8_t sign;
  uint16_t short_real;
  uint32_t nr;
  /*
   * Formare payload pentru fiecare tip de date
   */
  switch (message->data_type)
  {
    /*
     * Caz pentru tip payload "numar intreg fara semn"
     * cu valoarea tip_date = 0
     */
    case 0:
      sign = message->content[0];
      memcpy(&nr, message->content + 1, 4);
      nr = ntohl(nr);
      if (sign == 1)
        printf("-%d\n", nr);
      else 
        printf("%d\n", nr);
      break;

    /*
     * Caz pentru tip payload "Numar real pozitiv cu 2 zecimale"
     * cu valoarea tip_date = 1
     */
    case 1:
      memcpy(&short_real, message->content, 2);
      short_real = ntohs(short_real);
      printf("%.2f\n", short_real / (float)100);
      break;
    
    /*
     * Caz pentru tip payload "Numar real"
     * cu valoarea tip_date = 2
     */
    case 2:
      sign = message->content[0];
      memcpy(&nr, message->content + 1, 4);
      nr = ntohl(nr);
      uint8_t mod_pow = message->content[5];
      float result;
      result = nr / (float)power(mod_pow);
      if (sign == 1) 
        result = -result;
      printf("%.*f\n", mod_pow, result);
      break;

    /*
     * Caz pentru tip payload "Sir de caractere"
     * cu valoarea tip_date = 3
     */
    case 3:
      printf("%s\n", message->content);
      break;
  }
}

void run_client(int socket_tcp) {
  char buffer[MSG_MAXSIZE + 1];
  memset(buffer, 0, MSG_MAXSIZE + 1);
  int rc;
  /*
   * Sunt doar doi socketi posibili
   * STDIN si tcp (socket_tcp)
   */
  struct pollfd poll_fds[2];
  int num_clients = 2;

  poll_fds[0].fd = 0; 
  poll_fds[0].events = POLLIN;
  poll_fds[1].fd = socket_tcp; 
  poll_fds[1].events = POLLIN;

  while(1) {
    /*
     * Se asteapta un eveniment pe oricare dintre
     * socketii din poll_fds
     */
    rc = poll(poll_fds, num_clients, -1);
    DIE(rc < 0, "Poll invalid");

    /*
     * Daca se primeste o comanda de la STDIN
     * se verifica ce fel de comanda este
     */
    if (poll_fds[0].revents & POLLIN) {
      rc = read(0, buffer, sizeof(buffer));
      DIE(rc < 0, "No possible Read");

      /*
       * Daca este comanda "exit", inseamna ca clientul
       * doreste sa se deconecteze de la server si se
       * trimite un mesaj catre server pentru a fi anuntat
       */
      if(strncmp(buffer, "exit", 4) == 0){
        struct messenger client_mess;
        client_mess.subscribed = 2;
        rc = send(socket_tcp, &client_mess, sizeof(struct messenger), 0);
        DIE(rc < 0, "send");
        break;
      
      /*
       * Daca comanda este "subscribe" inseamna ca clientul
       * doreste sa fie abonat la un topic
       */
      } else if(strncmp(buffer, "subscribe", 9) == 0){
        struct messenger client_mess;
        /*
         * Se formeaza mesajul care va fi trimis catre server
         * pentru a anunta conectarea clientului la topicul 
         * mentionat, cu SF-ul dat
         */
        client_mess.subscribed = 1;

        char *token = strtok(buffer, " ");
        DIE(token == NULL, "strtok");

        token = strtok(NULL, " ");
        DIE(token == NULL, "No enough input");

        strcpy(client_mess.topic, token);

        token = strtok(NULL, " ");
        DIE(token == NULL, "Too little input");

        client_mess.sf = token[0] - '0';

        rc = send(socket_tcp, &client_mess, sizeof(struct messenger), 0);
        DIE(rc < 0, "send");
        printf("Subscribed to topic.\n");

      /*
       * Daca comanda este "unsubscribe" inseamna ca clientul
       * doreste sa fie dezabonat la un topic
       */
      } else if(strncmp(buffer, "unsubscribe", 11) == 0) {
        struct messenger client_mess;
        /*
         * Se formeaza mesajul care va fi trimis catre server
         * pentru a anunta deconectarea clientului de la topicul mentionat
         */
        client_mess.subscribed = 0;

        char *token = strtok(buffer, " ");
        DIE(token == NULL, "strtok2");

        token = strtok(NULL, " ");
        DIE(token == NULL, "Need one more argument");

        strcpy(client_mess.topic, token);
        rc = send(socket_tcp, &client_mess, sizeof(struct messenger), 0);
        DIE(rc < 0, "send");
        printf("Unsubscribed to topic.\n");
      }
    
    /*
     * Daca se primesc date de la server pe socketul de tcp
     * se afiseaza mesajul construit corespunzator
     */
    } else if (poll_fds[1].revents & POLLIN) {
      rc = recv(socket_tcp, buffer, sizeof(struct server_mess), 0);
      DIE(rc < 0, "Recv big fail");
      struct server_mess *message = (struct server_mess *)buffer;
      if(rc == 0)       
        break;

      show_message(message);
    }
  }
}

int main(int argc, char *argv[]) {
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);

  int socket_tcp;

  DIE(argc < 4, "Invalid number of arguments");

  // Parsez port-ul ca un numar
  uint16_t port;
  int rc = sscanf(argv[3], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Obtin un socket TCP pentru conectarea la server
  socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
  DIE(socket_tcp < 0, "Invalid tcp socket");

  /*
   * Completez in serv_addr adresa serverului, familia de adrese 
   * si portul pentru conectare
   */
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr);
  DIE(rc <= 0, "No inet_pton");

  // Dezactivez algoritmului Nagle
  int enable = 1;
	rc = setsockopt(socket_tcp, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
	DIE(rc < 0, "setcokopt fail");

  // Ma conectez la server
  rc = connect(socket_tcp, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "Lost Connection");

  // Trimit adresa ip a clientului catre server
  char client_id[11];
  memcpy(client_id, argv[1], strlen(argv[1]));
  rc = send(socket_tcp, client_id, strlen(client_id), 0);
  DIE(rc < 0, "Send impossible");

  run_client(socket_tcp);

  // Inchid conexiunea si socketul creat
  close(socket_tcp);

  return 0;
}