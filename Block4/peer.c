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

typedef struct internal_hash
{
    int socket;
    uint16_t hashId;
    Header header;
    Body body;
    int origin_socket;
    //? 0 = recieving lookup-reply
    //? 1 = recieving target-reply
    UT_hash_handle hh1;
    UT_hash_handle hh2;
} SocketHash;

SocketHash *socket_hashes_byHashId = NULL, *socket_hashes_bySocket = NULL;

PeerData peerdata; //? initialize global variable for peer information

fd_set master;
int fdmax;

/* -------------------------------------------------------------------------- */
/*                       //ANCHOR : INTERNAL HASH TABLE                       */
/* -------------------------------------------------------------------------- */

void printCountSocketHash()
{
    unsigned int amount_elements;
    amount_elements = HASH_CNT(hh1, socket_hashes_byHashId);
    printf("there are %u sockets in hashid and ", amount_elements);
    amount_elements = HASH_CNT(hh2, socket_hashes_bySocket);
    printf("%u sockets in the socket table!\n\n", amount_elements);
}

SocketHash *find_socketHash_byHashId(uint16_t *hash)
{
    fprintf(stderr, "searching for socketHashs %" PRIu16 " in hashtable..\n", *hash);
    SocketHash *socketHash = NULL;
    HASH_FIND(hh1, socket_hashes_byHashId, hash, sizeof(uint16_t), socketHash);
    return socketHash;
}

SocketHash *find_socketHash_bySocket(int *socket)
{
    fprintf(stderr, "searching for socketHashs with socket %d in hashtable..\n", *socket);
    SocketHash *socketHash = NULL;
    HASH_FIND(hh2, socket_hashes_bySocket, socket, sizeof(int), socketHash);
    return socketHash;
}

/* -------------------------------------------------------------------------- */

void delete_socketHash(uint16_t hashId, int socket)
{
    SocketHash *socketHash = NULL;
    socketHash = &hashId == NULL ? find_socketHash_bySocket(&socket) : find_socketHash_byHashId(&hashId);
    if (socketHash != NULL)
    {
        fprintf(stderr, "Removing socket %d with hash %" PRIu16 " from local HashTable!\n", socketHash->socket, hashId);
        HASH_DELETE(hh1, socket_hashes_byHashId, socketHash);
        HASH_DELETE(hh2, socket_hashes_bySocket, socketHash);
        fprintf(stderr, "Hash deleted!");
    }
    printCountSocketHash();
}

void delete_socketHash_bySocket(int socket)
{
    SocketHash *socketHash = NULL;
    socketHash = find_socketHash_bySocket(&socket);
    if (socketHash != NULL)
    {
        fprintf(stderr, "Removing socket %d with hash %d from local HashTable!\n", socketHash->socket, socket);

        HASH_DELETE(hh2, socket_hashes_byHashId, socketHash);
        fprintf(stderr, "Hash deleted!");
    }
}

/* -------------------------------------------------------------------------- */

void add_socketHash(int *socket, uint16_t *hashId, Header header, Body body, int origin_socket)
{
    if (find_socketHash_byHashId(hashId) == NULL)
    {
        SocketHash *newSocketHash = malloc(sizeof(SocketHash));
        newSocketHash->socket = *socket;
        newSocketHash->hashId = *hashId; //!FIXME pointer or value
        newSocketHash->header = header;
        newSocketHash->body = body;
        newSocketHash->origin_socket = origin_socket;

        HASH_ADD(hh1, socket_hashes_byHashId, hashId, sizeof(uint16_t), newSocketHash);
        HASH_ADD(hh2, socket_hashes_bySocket, socket, sizeof(int), newSocketHash);

        unsigned int amount_sockets;
        amount_sockets = HASH_CNT(hh1, socket_hashes_byHashId);
        printf("there are %u sockets in the table!\n\n", amount_sockets);
    }
    else if (find_socketHash_bySocket(socket) == NULL)
    {
        SocketHash *newSocketHash = malloc(sizeof(SocketHash));
        newSocketHash->socket = *socket;
        newSocketHash->hashId = *hashId; //!FIXME pointer or value
        newSocketHash->header = header;
        newSocketHash->body = body;
        newSocketHash->origin_socket = origin_socket;

        HASH_ADD(hh1, socket_hashes_byHashId, hashId, sizeof(uint16_t), newSocketHash);
        HASH_ADD(hh2, socket_hashes_bySocket, socket, sizeof(int), newSocketHash);

        unsigned int amount_sockets;
        amount_sockets = HASH_CNT(hh1, socket_hashes_byHashId);
        printf("there are %u sockets in the table!\n\n", amount_sockets);
    }
    else
    {
        fprintf(stderr, "SocketHash already in database...");
    }
    printCountSocketHash();
}

/* -------------------------------------------------------------------------- */

void update_socketHash(uint16_t *hashId, int origin_socket)
{
    SocketHash *socketHash = find_socketHash_byHashId(hashId);
    if (socketHash != NULL)
    {
        socketHash->origin_socket = origin_socket;
        HASH_ADD(hh1, socket_hashes_byHashId, hashId, sizeof(uint16_t), socketHash);
    }
}

/* -------------------------------------------------------------------------- */

void deleteAllSocketHashes()
{
    for (SocketHash *socketHash = socket_hashes_byHashId; socketHash != NULL; socketHash->hh1.next)
    {
        HASH_DELETE(hh1, socket_hashes_byHashId, socketHash);
        free(&(socketHash->socket));
        free(&(socketHash->hashId));
        Header *header = &socketHash->header;
        free(&(header->info));
        free(&(header->keyLength));
        free(&(header->valueLength));
        Body *body = &socketHash->body;
        free(&(body->key));
        free(&(body->value));
        free((socketHash));
    }
}

/* -------------------------------------------------------------------------- */
/*                      //ANCHOR: DISTRIBUTED HASH TABLE                      */
/* -------------------------------------------------------------------------- */

Element *elements = NULL;

/* -------------------------------------------------------------------------- */

void printCountElements()
{
    unsigned int amount_elements;
    amount_elements = HASH_COUNT(elements);
    printf("there are %u elements in the table!\n\n", amount_elements);
}

/* -------------------------------------------------------------------------- */

Element *find_element(Body *body, Header *header)
{
    Element *element = NULL;
    HASH_FIND_BYHASHVALUE(hh, elements, body->key, header->keyLength, 0, element);
    if (element == NULL)
        fprintf(stderr, "couldnt find element...\n");
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
        fprintf(stderr, "deleting element... ");
    }
    printCountElements();
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

        fprintf(stderr, "adding element... ");
        printCountElements();
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

uint16_t getHashId(Body body, uint16_t keyLength)
{
    uint16_t result = 0;
    if (body.key != NULL)
    {
        fprintf(stderr, "Getting Hash...\n");
        void *nboKey = networkByteOrder(body.key, keyLength);
        unsigned char *key = (unsigned char *)nboKey;

        result = (*key << 8);
        if (keyLength > 1)
        {
            result += *(key + 1);
        }
        fprintf(stderr, "Key: %s || %s \n", body.key, nboKey);
        fprintf(stderr, "HASHID: %" PRIu16 "\n", result);
        fprintf(stderr, "BITWISE: ");
        uint16_t r = result;
        int c = 0;
        for (int i = 0; i < 16; i++)
        {
            if (r & 1)
                fprintf(stderr, "1");
            else
                fprintf(stderr, "0");
            c += 1;
            if (c % 4 == 0)
                fprintf(stderr, " ");
            r >>= 1;
        }
        fprintf(stderr, "\n\n");
    }
    return result;
}

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
    fprintf(stderr, "returning socket... %d\n", sockfd);

    return sockfd > sockserv ? sockfd : sockserv;
}

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
    fprintf(stderr, "SENDING SET!... ");
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
    fprintf(stderr, "SENDING GET!... (Keylength: %" PRIu16 " ValueLength: %" PRIu32 ") \n", keyLength, valueLength);
    sendData(socket, (void *)(&info), 1);
    sendData(socket, (void *)(&keyLength), 2);
    sendData(socket, (void *)(&valueLength), 4);
    if (element != NULL)
    {
        fprintf(stderr, "key / value NULL? %d %d", key == NULL, value == NULL);
        sendData(socket, networkByteOrder(key, element->keyLength), element->keyLength);
        sendData(socket, networkByteOrder(value, element->valueLength), element->valueLength);
        fprintf(stderr, "Clients turn now..\n");
    }
}

/* ------------------------ //ANCHOR: FORWARD REQUEST ----------------------- */

void requestToTarget(Header *header, Body *body, int socket)
{
    uint16_t keyLength = htons(header->keyLength);
    uint32_t valueLength = htonl(header->valueLength);
    if (header != NULL && body != NULL)
        fprintf(stderr, "got the data from the request!.. Sending it on Socket %d\n", socket);

    sendData(&socket, (void *)&(header->info), sizeof(uint8_t));
    sendData(&socket, (void *)&(keyLength), sizeof(uint16_t));
    sendData(&socket, (void *)&(valueLength), sizeof(uint32_t));
    fprintf(stderr, "header sent");

    if (body != NULL)
    {
        fprintf(stderr, ", now sending value...\n");
        sendData(&socket, networkByteOrder(body->key, header->keyLength), header->keyLength);
        sendData(&socket, networkByteOrder(body->value, header->valueLength), header->valueLength);
    }
}

int sendControl(Control *controlMessage, Peer target)
{
    printControl(controlMessage);
    printControlDetails(target.ip, target.port);

    int targetfd = createSocket(target.ip, target.port, 0);

    uint16_t hashId = htons(controlMessage->hashId);
    uint16_t nodeId = htons(controlMessage->nodeId);
    uint32_t nodeIp = htonl(controlMessage->nodeIp);
    uint16_t nodePort = htons(controlMessage->nodePort);

    sendData(&targetfd, &(controlMessage->info), sizeof(uint8_t));
    sendData(&targetfd, &(hashId), sizeof(uint16_t));
    sendData(&targetfd, &(nodeId), sizeof(uint16_t));
    sendData(&targetfd, &(nodeIp), sizeof(uint32_t));
    sendData(&targetfd, &(nodePort), sizeof(uint16_t));

    fprintf(stderr, "Sent the control!");
    close(targetfd);
    return 0;
}

void sendRequest(int *socket, Body *body, Header *header)
{
    uint16_t hashId = getHashId(*body, header->keyLength);
    uint16_t selfId = peerdata.self.id;
    SocketHash *socketHash = malloc(sizeof(SocketHash));
    socketHash = find_socketHash_bySocket(socket);
    if (socketHash != NULL) // send reply to peer
    {
        if (*socket != socketHash->origin_socket)
        {
            fprintf(stderr, "RECIEVED ANSWER FOR REQUEST FROM PEER!");
            requestToTarget(header, body, socketHash->origin_socket);
            fprintf(stderr, "Answer sent...");

            close(*socket);
            FD_CLR(*socket, &master);
            close(socketHash->origin_socket);
            FD_CLR(socketHash->origin_socket, &master);
            delete_socketHash(NULL, socketHash->origin_socket);
            delete_socketHash(socketHash->hashId, NULL);
            return;
        }
    }
    free(socketHash);

    //NOTE: GOT THE NODE IN OWN HASH TABLE
    if (hashId <= selfId && hashId > peerdata.predecessor.id ||
        peerdata.predecessor.id > selfId && hashId > peerdata.predecessor.id)
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
            fprintf(stderr, "GET!\n");
            Element *element = find_element(body, header);
            sendGet(socket, element);
        }
        close(*socket);
        FD_CLR(*socket, &master);
    }
    else if (hashId > peerdata.self.id && hashId < peerdata.successor.id ||
             hashId > peerdata.self.id && hashId > peerdata.successor.id && peerdata.successor.id < peerdata.self.id)
    {
        int connect = createSocket(peerdata.successor.ip, peerdata.successor.port, 0);
        requestToTarget(header, body, connect);
        add_socketHash(&connect, &hashId, *header, *body, *socket);

        FD_SET(connect, &master);
        if (connect > fdmax)
            fdmax = connect;
        fprintf(stderr, "SUCCESSORS JOB!");
    }
    else
    {
        fprintf(stderr, "Not this Peers job, sending lookup..\n");

        Control *lookup = malloc(sizeof(Control));
        lookup->info = (uint8_t)129;
        lookup->hashId = hashId;
        lookup->nodeId = peerdata.self.id;
        lookup->nodeIp = peerdata.self.ip;
        lookup->nodePort = peerdata.self.port;

        sendControl(lookup, peerdata.successor);
        add_socketHash(socket, &hashId, *header, *body, *socket);
    }
}

void handleRequest(int *new_sock, Info *info) //passing adress of objects
{
    //? Case Lookup
    if (info->info == 129)
    {
        Control *control = recvControl(new_sock, info);
        if (control != NULL)
        {
            // ! ANSWER WITH SUCCESSOR AS CORRECT PEER
            if (control->hashId > peerdata.self.id && control->hashId < peerdata.successor.id ||
                control->hashId > peerdata.self.id && control->hashId > peerdata.successor.id && peerdata.successor.id < peerdata.self.id)
            {
                Peer target = {(uint16_t)0, control->nodeIp, control->nodePort};

                Control *reply = malloc(sizeof(Control));
                reply->info = (uint8_t)130;
                reply->hashId = control->hashId;
                reply->nodeId = peerdata.successor.id;
                reply->nodeIp = peerdata.successor.ip;
                reply->nodePort = peerdata.successor.port;

                if (sendControl(reply, target) == 0)
                {
                    fprintf(stderr, "Succesfully sent reply!");
                    close(*new_sock);
                }
                else
                {
                    fprintf(stderr, "%s/n", strerror(errno));
                }
                free(reply);
            }
            // ! FORWARD MESSAGE TO SUCCESSOR
            else
            {
                if (sendControl(control, peerdata.successor) == 0)
                {
                    fprintf(stderr, "Succesfully forwarded lookup!\n");
                }
                else
                {
                    fprintf(stderr, "%s/n", strerror(errno));
                }
            }
        }
        free(control);
        close(*new_sock);
        FD_CLR(*new_sock, &master);
    }

    //? Case Reply for own Lookup
    else if (info->info == 130)
    {
        Control *control = recvControl(new_sock, info);

        printControl(control);

        int connect = createSocket(control->nodeIp, control->nodePort, 0);
        SocketHash *sHash = malloc(sizeof(SocketHash));
        sHash = find_socketHash_byHashId(&(control->hashId));
        if (sHash != NULL) //? if case optional
        {
            //? TO PEER WHOS GOT THE DATA
            requestToTarget(&(sHash->header), &(sHash->body), connect);

            //NOTE: Adding socket to masterSet so that one can recieve the reply without blocking
            printCountSocketHash();
            delete_socketHash(sHash->hashId, NULL);
            add_socketHash(&connect, &(sHash->hashId), sHash->header, sHash->body, sHash->socket);
            printCountSocketHash();
            FD_SET(connect, &master);
            if (connect > fdmax)
                fdmax = connect;
        }
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
            fprintf(stderr, "\nHeader data revieced! Body Value Size: %d \n", sizeof(body->value));
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
    int i = endian();
    fprintf(stderr, "System is %s based!\n\n", i == 0 ? "little endian" : "big endian");
}

int main(int argc, char *argv[])
{
    setPeerData(argc, argv);

    fd_set read_fds;

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
                fprintf(stderr, "working on socket %d\n", curr_sock);
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
                        fprintf(stderr, "New connection from %s on"
                                        "socket %d on Server ID: %d\n",
                                inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&remoteaddr), connectIP, INET_ADDRSTRLEN),
                                new_sock, peerdata.self.id);
                    }
                }
                else
                {
                    Info *info = recvInfo(&curr_sock);
                    if (info != NULL)
                    {
                        handleRequest(&curr_sock, info);
                    }
                    else
                    {
                        if (close(curr_sock) == -1)
                            fprintf(stdout, "Socket %d has already been closed! Removing from master..", curr_sock);
                        FD_CLR(curr_sock, &master);
                    }
                }
            }
        }
    }

    deleteAll();

    return 0;
}
