#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define MAX_CONNECTIONS 1000
#define MSG_MAXSIZE 1552

/*
 * Structura definita pentru transmiterea 
 * mesajului de la client catre server
 */
struct messenger{
	/*
	 * subscribed = 0 <=> unsubscribe
	 * subscribed = 1 <=> subscribe
	 * subscribed = 2 <=> exit
	*/
	int subscribed;
	char topic[51];
	int sf;
};

/*
 * Structura definita pentru transmiterea 
 * mesajului de la server catre client
 */
struct server_mess{
	char ip[16];
	uint16_t port;
	char topic[51];
	char content[1501];
	int data_type;

	// Identificator tip (in functie de data_type)
	char identifier[11];
};

struct client{
	// id unic
	char id[11];

	// file descriptorul
	int sock;

	/*
	 * online = 1 <=> clientul este conectat
	 * online = 0 <=> clinetul este deconectat
	 */
	int online;

	// topicurile la care client[index] e abonat
	char topics_list[200][51];
	int topics_number;

	// mesajele pe care clientul le-a ratat
	struct server_mess losses[200];
	int losses_number;

	/*
	 * SF[i] = starea SF pentru topicul de pe 
	 * pozitia i din vectorul de topicuri
	 */
	int sf[200];
};

#endif