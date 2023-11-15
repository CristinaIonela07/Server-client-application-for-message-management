#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "helpers.h"

/*
 * Functie care verifica daca exista deja un client cu id-ul unique_id
 */
int check_presence(struct client *clients, int num_clients, char *unique_id) {
  for (int i = 0; i < num_clients; i++) {
    if (strcmp(clients[i].id, unique_id) == 0) {
      return i;
    }
  }
  return -1;
}

/*
 * Functie apelata la deconectarea unui client de la server
 */
void deconnect(struct client *clients, struct pollfd *poll_fds, int index, int num_clients) {
  printf("Client %s disconnected.\n", clients[index].id); 
  // Se actualizeaza starea clientului
  clients[index].online = 0;

  // Se elimina socketul clientului din lista de socketi
  int k = -1;
  for (int j = 0; j < num_clients; j++) {
    if (poll_fds[j].fd == clients[index].sock){
      k = j;
      break;
    }
     
  }
  if (k > -1) {
    close(poll_fds[k].fd);
    for (int j = k; j< num_clients; j++) {
      poll_fds[j] = poll_fds[j + 1];
    }
  }
}

void run_chat_multi_server(int socket_tcp, int socket_udp) {
  struct client *clients = malloc(MAX_CONNECTIONS * sizeof(struct client));

  struct pollfd poll_fds[MAX_CONNECTIONS];
  int num_clients = 3;
  int rc;

  // Setam socket-ul socket_tcp pentru ascultare
  rc = listen(socket_tcp, MAX_CONNECTIONS);
  DIE(rc < 0, "Listen gone WRONG");

  /*
   * Adaug cei 3 file descriptori de baza ai problemei: 
   * STDIN (cu fd = 0)
   * tcp (socket_tcp)
   * udp (socket_udp)
   */
  poll_fds[0].fd = 0; 
  poll_fds[0].events = POLLIN;

  poll_fds[1].fd = socket_tcp; 
  poll_fds[1].events = POLLIN;

  poll_fds[2].fd = socket_udp;
  poll_fds[2].events = POLLIN;
  char buffer[1551];
  while (1) {
    /*
     * Se asteapta un eveniment pe oricare dintre
     * file descritorii din poll_fds
     */
    rc = poll(poll_fds, num_clients, -1);
    DIE(rc < 0, "Poll invalid");

    /*
     * Se gaseste socketul care este pregatit de
     * actiune si i se verifica tipul
     */
    for (int i = 0; i < num_clients; i++) {
      if (poll_fds[i].revents & POLLIN) {
        /*
         * Daca file descriptorul are valoarea 0
         * se citeste input de la utilizator
         */
        if (poll_fds[i].fd == 0) {
          
          memset(buffer, 0, sizeof(buffer));
          rc = read(0, buffer, sizeof(buffer));
          DIE(rc < 0, "Read input gone WRONG");

          /*
           * Singura comanda acceptata de server este "exit"
           * Se trimite semnal de inchidere catre toti clientii
           */
          if (strncmp(buffer, "exit", 4) == 0) {
            for (int j = 0; j < num_clients; j++) {
              close(poll_fds[j].fd);
              DIE(rc < 0, "Don't wanna shut down");
            }
            return;

          } else {
            /*
             * Orice alta comanda nu este permisa
             * Se genereaza eroare
             */
            fprintf(stderr, "Invalid command");
            exit(EXIT_FAILURE);
          }
        
        /*
         * Daca socketul care este pregatit este cel de tcp
         * pe care se asculta conexiuni
         */ 
        } else if (poll_fds[i].fd == socket_tcp) {
          /*
           * A venit o cerere de conexiune pe socketul inactiv 
           * (cel cu listen), pe care serverul o accepta
           */ 
          struct sockaddr_in cli_addr;
          socklen_t cli_len = sizeof(cli_addr);

          int newsockfd = accept(socket_tcp, (struct sockaddr *)&cli_addr,
                                  &cli_len);
          DIE(newsockfd < 0, "Don't Accept");

          /*
           * Se adauga noul socket intors de accept() la
           * multimea descriptorilor de citire
           */
          poll_fds[num_clients].fd = newsockfd;
          poll_fds[num_clients].events = POLLIN;
          num_clients++;

          // Se primeste id-ul utilizatorului (unic)
          char unique_id[11];
          memset(unique_id, 0, 11);
          int n = recv(newsockfd, unique_id, 10, 0);
          DIE(n < 0, "Recv invalid");

          // Se verifica daca exista deja un client cu acel id
          int index = check_presence(clients, num_clients, unique_id);

          /*
           * Daca nu exista, se adauga clientul in lista de clienti
           */
          if (index == -1) {
            printf("New client %s connected from %s:%d\n", unique_id,
                   inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

            strcpy(clients[num_clients].id, unique_id);
            clients[num_clients].sock = newsockfd;
            clients[num_clients].online = 1;
            clients[num_clients].topics_number = 0;
            clients[num_clients].losses_number = 0;
            num_clients++;

          /*
           * Daca exista un alt client deja conectat cu acelasi id,
           * se inchide clientul
           */
          } else {
            if (clients[index].online == 1) {
              printf("Client %s already connected.\n", unique_id);
              close(newsockfd);

            /*
             * Daca clientul exista, dar este deconectat,
             * atunci se realizeaza conectarea
             */
            } else {
              clients[index].online = 1;
              /*
               * Daca clientul se reconecteaza pe alt socket
               * decat cel pe care s-a conectat anterior
               */
              if (newsockfd != clients[index].sock) {
                int k = -1 ;

                // Se cauta socketul anterior
                for (int j = 0; j < num_clients - 1; j++) {
                  if (poll_fds[j].fd == clients[index].sock) {
                    k = j;
                    break;
                  }
                }

                // Se sterge socketul din lista de socketi
                if (k != -1)
                  for (int l = k; l < num_clients; l++)
                    poll_fds[l] = poll_fds[l + 1];
              }

              // Se actualizeaza socketul clientului
              clients[index].sock = newsockfd;
              
              /*
               * Pentru fiecare topic la care este abonat clientul
               */
              for (int l = 0; l < clients[index].topics_number; l++) {
                /*
                * Clientul primeste toate mesajele pe care 
                * le-a ratat cat a fost deconectat de la respectivul topic
                */
                for (int k = 0; k < clients[index].losses_number; k++) {
                  if (strcmp(clients[index].losses[k].topic, 
                      clients[index].topics_list[l]) == 0) {
                    rc = send(newsockfd, &clients[index].losses[k], 
                              sizeof(struct server_mess), 0);
                    DIE(rc < 0, "Send remained big fail");
                  }
                }
              }
              clients[index].losses_number = 0;
              printf("New client %s connected from %s:%d\n", unique_id,
                     inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            }
          }
        
        // Daca socketul pe care avem eveniment este cel de udp
        } else if (poll_fds[i].fd == socket_udp) {
          struct sockaddr_in serv_addr;
          struct server_mess message;
          socklen_t serv_len = sizeof(serv_addr);
          memset(&message, 0, sizeof(message));
          memset(buffer, 0, sizeof(buffer));
          
          /*
           * Se primeste mesajul de la clientul udp
           * rc va fi egal cu numarul de biti primiti
           */ 
          rc = recvfrom(socket_udp, buffer, sizeof(buffer), 0,
                (struct sockaddr *)&serv_addr, &serv_len);
          DIE(rc < 0, "Recvfrom invalid");

          // length_content = lungimea continutului mesajului
          int length_content = rc - 51;

          // Se construieste mesajul care va fi trimis conform cerintei
          memcpy(message.topic, buffer, 50);
          memcpy(&(message.data_type), buffer + 50, 1);

          memcpy(message.content, buffer + 51, length_content);
          message.content[length_content] = '\0';

          /*
           * In functie de tipul de date
           * Se seteaza identificatorul corespunzator
           */
          if (message.data_type == 0) 
            strcpy(message.identifier, "INT");
          else 
            if (message.data_type == 1) 
              strcpy(message.identifier, "SHORT_REAL");
          else 
            if (message.data_type == 2) 
              strcpy(message.identifier, "FLOAT");
          else 
            if (message.data_type == 3) 
              strcpy(message.identifier, "STRING");

          /*
           * Pentru fiecare client, se verifica daca
           * este abonat la topicul primit
           */
          for (int j = 0; j < num_clients; j++)
            for (int k = 0; k < clients[j].topics_number; k++)

              /*
               * Daca este abonat si este conectat
               * se trimite mesajul
               */
              if (strcmp(clients[j].topics_list[k], message.topic) == 0 
                          && clients[j].online) {
                rc = send(clients[j].sock, &message, sizeof(message), 0);
                DIE(rc < 0, "Send not good");
              }

              /*
               * Daca este abonat, dar deconectat, si are sf-ul activat
               * se adauga mesajul in lista de mesaje pierdute
               */
              else if (strcmp(clients[j].topics_list[k], message.topic) == 0 
                          && clients[j].online == 0 && clients[j].sf[k] == 1) {
                clients[j].losses[clients[j].losses_number] = message;
                clients[j].losses_number++;
              }
        } else {
          // Se primeste mesaj de la client
          struct messenger client_mess;
          rc = recv(poll_fds[i].fd, &client_mess, sizeof(struct messenger), 0);
          DIE(rc < 0, "Recv STOP!");

          // Se identifica clientul care a trimis mesajul
          int index = -1;
          for (int k = 0; k < num_clients && index == -1; k++)
            if (clients[k].sock == poll_fds[i].fd) 
              index = k;

          // Daca s-a inchis conexiunea
          if (rc == 0) {
            deconnect(clients, poll_fds, index, num_clients);
          } else {
            /*
             * Daca clientul doreste sa faca subscribe
             * la un topic, se adauga clientului un nou topic
             */
            if (client_mess.subscribed == 1) {
              strcpy(clients[index].topics_list[clients[index].topics_number], client_mess.topic);
              clients[index].topics_number++;
              clients[index].sf[clients[index].topics_number] = client_mess.sf;
            }
            /*
             * Daca clientul doreste sa faca unsubscribe la un topic
             * se sterge din lista de topicuri a clientului respectivul
             * topic
             */ 
            if (client_mess.subscribed == 0) {
              int j = -1;
              for (int k = 0; k < clients[index].topics_number; k++) {
                  if (strcmp(clients[index].topics_list[j], client_mess.topic) == 0) {
                    j = k;
                    break;
                  }
              }

              /*
               * Se sterge si valoarea SF-ului corespunzator topicului
               * din vectorul de SF-uri
               */
              if (j > -1) {
                while (j < clients[index].topics_number - 1) {
                  strcpy(clients[index].topics_list[j], clients[index].topics_list[j + 1]);
                  clients[index].sf[j] = clients[index].sf[j+1];
                  j++;
                }
                clients[index].topics_number--;
              }
            }

            // Daca clientul doreste sa se deconecteze de la server
            if (client_mess.subscribed == 2) {
              deconnect(clients, poll_fds, index, num_clients);
            }
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);

  DIE(argc < 2, "Invalid number of arguments");

  // Parsez port-ul ca un numar
  uint16_t port;
  int rc = sscanf(argv[1], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  // Obtin un socket TCP pentru receptionarea conexiunilor
  int socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
  DIE(socket_tcp < 0, "Socket tcp invalid");

  // Obtin un socket UDP
  int socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(socket_udp < 0, "Socket udp invalid");

  /* 
   * Completez in serv_addr adresa serverului,
   * familia de adrese si portul pentru conectare
   */
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  // Dezactivez algoritmului Nagle
  int enable = 1;
	rc = setsockopt(socket_tcp, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
	DIE(rc < 0, "setsockopt fail");

  // Asociez adresa serverului cu socketii creati folosind bind
  rc = bind(socket_tcp, (const struct sockaddr *)&serv_addr, socket_len);
  DIE(rc < 0, "Bind tcp WRONG");

  rc = bind(socket_udp, (const struct sockaddr *)&serv_addr, socket_len);
  DIE(rc < 0, "Bind udp WRONG");

  run_chat_multi_server(socket_tcp, socket_udp);

  // Inchid socketii
  close(socket_tcp);
  close(socket_udp);

  return 0;
}