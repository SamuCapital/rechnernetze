#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <inttypes.h>
#include "protocol.h"

char *readStdin(int *valuelength)
{
    int max = 100;
    int c;
    *valuelength = 0;
    char *buffer = malloc(sizeof(char) * max);
    while ((c = getchar()) != EOF)
    {
        memcpy((buffer + *(valuelength)), &c, 1);
        *valuelength = *valuelength + 1;
        if (*valuelength >= max)
        {
            max += 100;
            buffer = realloc(buffer, sizeof(char) * max);
        }
    }
    return buffer;
}

void writeStdout(char *buffer, int *valuelength)
{
    char c;
    int i = 0;
    while (i < *valuelength)
    {
        c = putchar(buffer[i]);
        i++;
    }
}

int main(int argc, char *argv[])
{
    int max = 100;
    int valuelength = 0;

    char *buffer = NULL;
    int c;
    int sockfd;
    struct addrinfo hints;
    struct addrinfo *servinfo, *result;

    printf("Client running!\n");

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int s = getaddrinfo(argv[1], argv[2], &hints, &servinfo);
    if (s != 0)
    {
        fprintf(stderr, "getadressinfo:%s\n", gai_strerror(s));
    }

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

    freeaddrinfo(servinfo);
    if (result == NULL)
    {
        fprintf(stderr, "%s\n", strerror(errno));
        return (1);
    }
    char *ip = argv[1];
    //strcpy(ip, argv[1]);

    char *port = argv[2];
    // strcpy(port, argv[2]);

    char *method = argv[3];
    //strcpy(method, argv[3]);

    char *path = argv[4];
    //strcpy(path, argv[4]);

    if (memcmp(method, "SET", 3) == 0)
    {
        //read File
        buffer = readStdin(&valuelength);

        Info *info = malloc(sizeof(Info));
        info->info = (uint8_t)2;
        //setHeader
        Header *setHeader = malloc(sizeof(Header));
        setHeader->info = (uint8_t)2;
        setHeader->keyLength = htons((uint16_t)(strlen(path)));
        setHeader->valueLength = htonl((uint32_t)(valuelength));

        Body *setBody = malloc(sizeof(Body));
        setBody->key = path;
        setBody->value = buffer;

        sendData(&sockfd, &(setHeader->info), sizeof(uint8_t));
        sendData(&sockfd, &(setHeader->keyLength), sizeof(uint16_t));
        sendData(&sockfd, &(setHeader->valueLength), sizeof(uint32_t));

        //sendBody

        sendData(&sockfd, setBody->key, strlen(path));
        sendData(&sockfd, setBody->value, valuelength);

        //receive header with ack bit
        setHeader = rcvHeader(&sockfd, info);

        free(buffer);
        free(setHeader);
        valuelength = 0;
    }

    if (memcmp(method, "GET", 3) == 0)
    {

        printf("let's connect to %d\n", argv[2]);

        Info *info = malloc(sizeof(Info));
        info->info = (uint8_t)4;

        Header *getHeader = malloc(sizeof(Header));
        getHeader->info = info->info;
        getHeader->keyLength = htons((uint16_t)(strlen(path)));
        getHeader->valueLength = htonl((uint32_t)0);

        printHeader(getHeader);

        Body *getBody = malloc(sizeof(Body));
        getBody->key = path;
        getBody->value = NULL; //soll in den buffer schreiben

        sendData(&sockfd, &(getHeader->info), sizeof(uint8_t));
        sendData(&sockfd, &(getHeader->keyLength), sizeof(uint16_t));
        sendData(&sockfd, &(getHeader->valueLength), sizeof(uint32_t));
        sendData(&sockfd, getBody->key, getHeader->keyLength);

        free(info);

        Info *infoRecv = malloc(sizeof(info));
        Header *recvHeader = malloc(sizeof(Header));
        infoRecv = recvInfo(&sockfd);
        recvHeader = rcvHeader(&sockfd, infoRecv);
        fprintf(stderr, "%s\n", "error AFTER HEADER\n");
        fprintf(stderr, "Value length: %" PRIu32 "\n", "!", recvHeader->valueLength);

        printHeader(recvHeader);

        getBody = readBody(&sockfd, recvHeader);
        fprintf(stderr, "%s\n", "error AFTER body\n");

        writeStdout(getBody->value, &(recvHeader->valueLength));
        free(buffer);
        valuelength = 0;
        free(getBody);
        free(getHeader);

        free(recvHeader);
    }

    if (memcmp(method, "DELETE", 6) == 0)
    {

        Info *info = malloc(sizeof(Info));
        info->info = (uint8_t)1;

        Header *delHeader = malloc(sizeof(Header));
        delHeader->info = (uint8_t)1;
        delHeader->keyLength = htons((uint16_t)strlen(path));
        delHeader->valueLength = htonl((uint32_t)0);

        Body *delBody = malloc(sizeof(Body));
        delBody->key = path;
        delBody->value = NULL;

        sendData(&sockfd, &(delHeader->info), sizeof(uint8_t));
        sendData(&sockfd, &(delHeader->keyLength), sizeof(uint16_t));
        sendData(&sockfd, &(delHeader->valueLength), sizeof(uint32_t));

        sendData(&sockfd, delBody->key, strlen(path));

        delHeader = rcvHeader(&sockfd, info);

        free(delHeader);
        free(delBody);
    }
    printf("Data sent! Closing...\n");

    close(sockfd);
}
