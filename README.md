# Server-client-application-for-message-management
Am construit 3 structuri: una pentru mesajele trimise de la client 
catre server (struct messenger), una pentru mesajele trimise de la
server catre client (struct server_mess), si una pentru reprezentarea
clientului, cu toate datele necesare.

Struct messenger contine campurile: subscribed (care exprima daca clientul
doreste sa se aboneze la un topic (1), sa se dezaboneze de la un topic(0), sau 
doar sa paraseasca serverul(2)), topic pentru a retine topicul, si sf pentru
a retine daca se doreste primirea de mesaje de la topic chiar daca si 
atunci cand e deconectat.

Struct server_mess contine campurile: ip si port (pentru a identifica ip-ul
si portul de pe care s-a publicat mesajul), topic si content (pentru topicul
si continutul mesajului), data_type pentru tipul de date, si identifier
pentru tipul de date transformat in identificator de tip.

Struct client contine: un id (unic pentru fiecare client), socketul pe care
s-a realizat conexiunea, variabila online pentru a retine daca clientul este
conectat sau nu, o lista de topicuri la care este abonat clientul, o lista
pentru SF-ul fiecarui topic, si o lista de mesaje pierdute din cauza ca 
s-au trimis mesaje pe un topic cat timp el a fost deconectat.

SERVER.C
Se parseaza portul(port) si se obtin socketii de tcp si udp (socket_tcp, 
socket_udp), precum si adresa serverului, familia de adrese si portul 
pentru conectare (serv_addr).

Pentru ca comenzile si mesajele trimise sa aiba efect imediat, am dezactivat 
algoritmului Nagle (cu TCP_NODELAY). Am asociat adresa serverului cu 
ambii socketii creati folosind bind.

Urmeaza ca in functia run_chat_multiserver sa implementez efectiv protocoalele.
Am creat o lista de clienti si o lista de socketi pentru a tine mai usor
evidenta. Cand se gaseste un socket care este pregatit de o actiune, i se
verifica tipul.

Daca socketul este de tip STDIN, se citeste input de la utilizator. Singura 
comanda permisa este "exit" care inchide toti clientii/socketii. 

Daca socketul este de tip tcp, inseamna ca a venit o cerere de conexiune pe 
socketul inactiv (cel cu listen), pe care serverul o accepta. Se adauga socketul
in multimea de socketi. Se primeste id-ul clientului si se verifica daca
este prezent in retea. Daca nu exista deloc, atunci se adauga in lista de clienti.
Daca exista, dar este deconectat, se inchide socketul. Daca este deconectat,
inseamna ca clientul incearca sa se reconecteze. El revine online si daca se
conecteaza pe un alt socket decat cel pe care s-a conectat initial, acesta
se inchide, si i se atribuie noul socket. Urmeaza trimiterea tuturor mesajelor
ratate cat a fost deconectat.

Daca socketul este de tip udp, se primeste mesajul de la clientul udp si se
construieste mesajul care va fi trimis mai departe. Pentru fiecare client, se verifica
daca este abonat la topicul primit. Daca este abonat sie ste conectat, i se trimite
mesajul. Daca este abonat, dar dezabonat, si cu sf-ul pe 1, se adauga mesajul in
lista de mesaje pierdute din cauza ca este dezabonat.

Daca se primeste mesaj de un client, se identifica care client a trimis mesajul.
Daca nu se primesc date pe socket, conexiunea a fost inchisa si se deconecteaza 
clientul. Posibilele mesajele primite de la client sunt de subscribe, unsubscribe si exit. 
Daca mesajul este de subscribe, atunci se adauga topicul in lista de topicuri
a clientului. Daca mesajul este de unsubscribe, atunci se sterge topicul si
de asemenea sf-ul corespunzator. Daca mesajul este de exit, atunci clientul
este deconectat. Pentru deconectare, am creat o functie, epntru a evita codul 
duplicat.


SUBSCRIBER.C
Similar server.c, se parseaza portul(port) si se obtine socketul de tcp
(socket_tcp), precum si adresa serverului, familia de adrese si portul 
pentru conectare (serv_addr). Pentru ca comenzile si mesajele trimise sa aiba 
efect imediat, am dezactivat algoritmului Nagle (cu TCP_NODELAY). Urmeaza 
conectarea la server cu functia connect si primiterea id-ului clientului catre
server. Daca se primeste o comanda de la stdin, e verifica ce comanda este.

Daca este comanda "exit", inseamna ca clientul doreste sa se deconecteze de 
la server si se trimite un mesaj (cu subscribed = 2) catre server pentru a fi 
anuntat. 

Daca comanda este "subscribe" inseamna ca clientul doreste sa fie abonat la un 
topic. Se formeaza mesajul care va fi trimis catre server pentru a anunta conectarea clientului la topicul mentionat, cu SF-ul dat (subscribed = 1). 

Daca comanda este "unsubscribe" inseamna ca clientul doreste sa fie dezabonat la un topic. Se formeaza mesajul care va fi trimis catre server pentru a anunta deconectarea clientului de la topicul mentionat (subscribed = 0).

Daca se primesc date de la server pe socketul de tcp, se afiseaza mesajul corespunzator construit cu functia show_message. In functie de tipul de date, se afiseaza mesajul potrivit cerintei.
