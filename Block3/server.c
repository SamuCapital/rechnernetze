#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/nameser_compat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <inttypes.h>
#include "protocol.h"
#include "src/uthash.h"

typedef struct my_peer
{
    uint16_t id;
    uint32_t ip;
    uint16_t port;
} Peer;

typedef struct my_peerdata
{
    Peer predecessor;
    Peer self;
    Peer successor;

} PeerData;

typedef struct my_struct
{
    void *key;
    void *value;
    uint16_t keyLength;
    uint32_t valueLength;
    UT_hash_handle hh;
    // makes this structure hashable

} Element;

//TODO: SETUP INTERNAL HASH TABLE

// typedef struct internal_hash
// {
//     int Socket;
//     uint16_t hashId;
//     UT_hash_handle hh;
// } SocketHash;

// SocketHash *socket_hashes = NULL;

// SocketHash *find_hash(uint16_t hash)
// {
//     SocketHash *sh = NULL;
//     HASH_FIND_BYHASHVALUE(hh, socket_hashes, hash, 16, 0, sh);
//     return sh;
// }

PeerData peerdata; //initialize global variable for peer information

/* -------------------------------------------------------------------------- */
/*                      //ANCHOR: DISTRIBUTED HASH TABLE                      */
/* -------------------------------------------------------------------------- */

Element *elements = NULL;

Element *find_element(Body *body, Header *header)
{
    Element *element = NULL;
    HASH_FIND_BYHASHVALUE(hh, elements, body->key, header->keyLength, 0, element);
    return element;
}

/* -------------------------------------------------------------------------- */

void delete_element(Body *body, Header *header)
{
    Element *element = NULL;
    element = find_element(body, header);
    if (element != NULL)
    {
        HASH_DEL(elements, element);
        free(element->key);
        free(element->value);
        free((element));
    }
}

/* -------------------------------------------------------------------------- */

void add_element(Body *body, Header *header)
{
    if (find_element(body, header) == NULL)
    {
        Element *newElement = malloc(sizeof(Element));
        newElement->key = body->key;
        newElement->value = body->value;
        newElement->keyLength = header->keyLength;
        newElement->valueLength = header->valueLength;
        HASH_ADD_KEYPTR_BYHASHVALUE(hh, elements, newElement->key, newElement->keyLength, 0, newElement);
    }
}

/* -------------------------------------------------------------------------- */

void deleteAll()
{
    for (Element *s = elements; s != NULL; s->hh.next)
    {
        HASH_DEL(elements, s);
        free(s->key);
        free(s->value);
        free((s));
    }
}

/* ------------------------- END OF DATA MANAGEMENT ------------------------- */

/* -------------------------------------------------------------------------- */
/*                        //ANCHOR: RESPONSE MANAGEMENT                       */
/* -------------------------------------------------------------------------- */

void sendDelete(int *socket)
{
    uint8_t info;
    uint16_t keyLength;
    uint32_t valueLength;
    info = 9;
    valueLength = 0;
    keyLength = 0;
    sendData(socket, (void *)(&info), 1);
    sendData(socket, (void *)(&keyLength), 2);
    sendData(socket, (void *)(&valueLength), 4);
}

/* -------------------------------------------------------------------------- */

void sendSet(int *socket)
{
    uint8_t info;
    uint16_t keyLength;
    uint32_t valueLength;
    info = 10;
    valueLength = 0;
    keyLength = 0;
    sendData(socket, (void *)(&info), 1);
    sendData(socket, (void *)(&keyLength), 2);
    sendData(socket, (void *)(&valueLength), 4);
}

/* -------------------------------------------------------------------------- */

void sendGet(int *socket, Element *element)
{
    uint8_t info = 4; // in case the key ist not in the hash table (the AKC Bit is not set )
    uint16_t keyLength = 0;
    uint32_t valueLength = 0;
    void *key = NULL;
    void *value = NULL;
    if (element != NULL)
    {
        key = element->key;
        value = element->value;
        info = 12;
        keyLength = htons(element->keyLength);
        valueLength = htonl(element->valueLength);
    }
    sendData(socket, (void *)(&info), 1);
    sendData(socket, (void *)(&keyLength), 2);
    sendData(socket, (void *)(&valueLength), 4);
    if (element != NULL)
    {
        sendData(socket, key, element->keyLength);
        sendData(socket, value, element->valueLength);
        printf("SEND GET! Clients turn now..\n");
    }
}

/* -------------------------------------------------------------------------- */

void sendRequest(int *socket, Body *body, Header *header)
{
    if (header->info == 1)
    {
        delete_element(body, header);
        sendDelete(socket);
        return;
    }
    else if (header->info == 2)
    {
        add_element(body, header);
        sendSet(socket);
    }
    else if (header->info == 4)
    {
        printf("GET!\n");
        Element *element = find_element(body, header);
        sendGet(socket, element);
    }
}

/* ----------------------- END OF RESPONSE MANAGEMENT ----------------------- */

/* -------------------------------------------------------------------------- */
/*     //NOTE: CREATES SOCKET AND RETURNS IT, input @clientOrServer 0 or 1    */
/* -------------------------------------------------------------------------- */

int createSocket(uint32_t ip, uint16_t port, int clientOrServer)
{
    int sockfd = -1, sockserv = -1;
    struct addrinfo hints;
    struct addrinfo *servinfo, *result;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    char connectIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ip), connectIp, INET_ADDRSTRLEN);
    char connectPort[16];
    sprintf(connectPort, "%d", port);

    int s = getaddrinfo(connectIp, connectPort, &hints, &servinfo);
    if (s != 0)
    {
        fprintf(stderr, "getadressinfo:%s\n", gai_strerror(s));
    }

    //? 0 to create Client Socket, 1 for Server
    switch (clientOrServer)
    {

        /* -------------------------- CREATES CLIENT SOCKET ------------------------- */

    case 0:
        for (result = servinfo; result != NULL; result = result->ai_next)
        {
            sockfd = socket(result->ai_family, result->ai_socktype,
                            result->ai_protocol);
            if (sockfd == -1)
            {
                close(sockfd);
                continue;
            }
            if (connect(sockfd, result->ai_addr, result->ai_addrlen) != -1)
            {
                break;
            }
        }
        break;

        /* -------------------------- CREATES SERVER SOCKET ------------------------- */

    case 1:
        for (result = servinfo; result != NULL; result = result->ai_next)
        {
            sockserv = socket(result->ai_family, result->ai_socktype,
                              result->ai_protocol);
            if (sockserv == -1)
            {
                close(sockserv);
                continue;
            }

            if (bind(sockserv, result->ai_addr, result->ai_addrlen) >= 0)
            {
                break;
            }
        }
        break;

    default:
        break;
    }

    freeaddrinfo(servinfo);
    if (result == NULL)
    {
        fprintf(stderr, "%s\n", strerror(errno));
        return (-1);
    }
    printf("returning socket... %d\n", sockfd);

    return sockfd > sockserv ? sockfd : sockserv;
}

/* -------------------------------------------------------------------------- */
/*                     //NOTE: SENDS DHT INTERNAL MESSAGE                     */
/* -------------------------------------------------------------------------- */

int sendControl(Control *reply, Peer target)
{
    int targetfd = createSocket(target.ip, target.port, 0);

    uint16_t hashId = htons(reply->hashId);
    uint16_t nodeId = htons(reply->nodeId);
    uint32_t nodeIp = htonl(reply->nodeIp);
    uint16_t nodePort = htons(reply->nodePort);

    sendData(&targetfd, &(reply->info), sizeof(uint8_t));
    sendData(&targetfd, &(hashId), sizeof(uint16_t));
    sendData(&targetfd, &(nodeId), sizeof(uint16_t));
    sendData(&targetfd, &(nodeIp), sizeof(uint32_t));
    sendData(&targetfd, &(nodePort), sizeof(uint16_t));

    close(targetfd);
    return 0;
}

void handleRequest(int *new_sock, Info *info) //passing adress of objects
{
    //? Case Lookup
    if (info->info == 129)
    {
        Control *control = recvControl(new_sock, info);
        if (control != NULL)
        {

            if (control->hashId > peerdata.self.id && control->hashId < peerdata.successor.id || control->hashId > peerdata.self.id && control->hashId > peerdata.successor.id && peerdata.successor.id < peerdata.self.id)
            {
                // // //TODO: SEND REPLY WITH DATA OF SUCCESSOR

                Peer target = {(uint16_t)0, control->nodeIp, control->nodePort};

                Control *reply = malloc(sizeof(Control));
                reply->info = (uint8_t)130;
                reply->hashId = control->hashId;
                reply->nodeId = peerdata.successor.id;
                reply->nodeIp = peerdata.successor.ip;
                reply->nodePort = peerdata.successor.port;

                if (sendControl(reply, target) == 0)
                {
                    printf("Succesfully sent reply!");
                }
                else
                {
                    fprintf(stderr, "%s/n", strerror(errno));
                }

                free(reply);
            }
            else if (control->hashId > peerdata.successor.id)
            {
                if (sendControl(control, peerdata.successor) == 0)
                {
                    printf("Succesfully forwarded lookup!");
                }
                else
                {
                    //TODO: HANDLE ERROR
                    fprintf(stderr, "%s/n", strerror(errno));
                }
            }
        }
        free(control);
    }

    //? Case Reply for own Lookup
    else if (info->info == 130)
    {
        Control *control = recvControl(new_sock, info);

        // TODO: HANDLE REPLY

        free(control);
    }

    //? Case request (GET/SET/DELETE)
    else
    {
        Header *header = rcvHeader(new_sock, info); // to receive the data from Header
        if (header != NULL)
        {
            printHeader(header);
            Body *body = readBody(new_sock, header);
            printf("\nHeader data revieced! Body Value Size: %d \n", sizeof(body->value));
            if (body != NULL)
            {
                sendRequest(new_sock, body, header); // if the receive of the data of the header succeed , read the data of Body
                free(body);
            }
            free(header);
        }
    }
}

//NOTE: Not really neccessary, but saves time for implementation of IPV6

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

/* -------------------------------------------------------------------------- */
/*                 INITIALIZES PEER DATA IN PROCESSABLE FORMAT                */
/* -------------------------------------------------------------------------- */

void setPeerData(int argc, char *argv[])
{
    if (argc < 10)
    {
        fprintf(stderr, "zu wenig argumente!\n");
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "server running\n");

    struct sockaddr_in sa;
    inet_pton(AF_INET, argv[2], &(sa.sin_addr));
    Peer pre = {(uint16_t)atoi(argv[1]), sa.sin_addr.s_addr, (uint16_t)atoi(argv[3])};
    inet_pton(AF_INET, argv[5], &(sa.sin_addr));
    Peer self = {(uint16_t)atoi(argv[4]), sa.sin_addr.s_addr, (uint16_t)atoi(argv[6])};
    inet_pton(AF_INET, argv[8], &(sa.sin_addr));
    Peer suc = {(uint16_t)atoi(argv[7]), sa.sin_addr.s_addr, (uint16_t)atoi(argv[9])};

    peerdata.predecessor = pre;
    peerdata.self = self;
    peerdata.successor = suc;
}

int main(int argc, char *argv[])
{

    setPeerData(argc, argv);

    fd_set master, read_fds;
    // fd_set read_fds;
    int fdmax;

    socklen_t addr_size;
    struct sockaddr_storage their_addr, remoteaddr;
    socklen_t addrlen;

    int new_sock;
    int yes = 1;
    int curr_sock, j, rv;
    char connectIP[INET_ADDRSTRLEN];

    int listener = createSocket(peerdata.self.ip, peerdata.self.port, 1);

    // 10 connections allowed in this case
    if (listen(listener, 10) == -1)
    {
        fprintf(stderr, "%s/n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    FD_SET(listener, &master);
    fdmax = listener;

    //ANCHOR: Select server

    while (1)
    {
        read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }

        for (curr_sock = 3; curr_sock <= fdmax; curr_sock++)
        {
            if (FD_ISSET(curr_sock, &read_fds))
            {
                if (curr_sock == listener)
                {
                    addrlen = sizeof(remoteaddr);
                    new_sock = accept(listener,
                                      (struct sockaddr *)&remoteaddr,
                                      &addrlen);
                    if (new_sock == -1)
                    {
                        perror("accepting");
                    }
                    else
                    {
                        FD_SET(new_sock, &master);
                        if (new_sock > fdmax)
                            fdmax = new_sock;
                        printf("New connection from %s on"
                               "socket %d\n",
                               inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&remoteaddr), connectIP, INET_ADDRSTRLEN),
                               new_sock);
                    }
                }
                else
                {
                    Info *info = recvInfo(&curr_sock);
                    if (info != NULL)
                    {
                        handleRequest(&curr_sock, info);
                        //TODO: ADD TO LOCAL HASHTABLE AND HANDLE SOCKET INSTEAD OF CLOSING IMMEDIATELY
                        close(curr_sock);
                        FD_CLR(curr_sock, &master);
                    }
                    else
                    {
                        close(curr_sock);
                        FD_CLR(curr_sock, &master);
                    }
                }
            }
        }
    }

    // while (1)
    // {
    //     addr_size = sizeof their_addr;
    //     new_sock = accept(listener, (struct sockaddr *)&their_addr, &addr_size);
    //     printf("got a socket!");
    //     if (new_sock == -1)
    //     {
    //         fprintf(stderr, "%s/n", strerror(errno));
    //     }

    //     Info *info = recvInfo(&new_sock);
    //     handleRequest(&new_sock, info);
    //     close(new_sock);
    // }

    deleteAll();

    return 0;
}
