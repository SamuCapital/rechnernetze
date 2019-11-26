
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/nameser_compat.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <inttypes.h>
#include "protocol.h"
#include "uthash.h"

// #define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
// #define BYTE_TO_BINARY(byte)       \
//     (byte & 0x80 ? '1' : '0'),     \
//         (byte & 0x40 ? '1' : '0'), \
//         (byte & 0x20 ? '1' : '0'), \
//         (byte & 0x10 ? '1' : '0'), \
//         (byte & 0x08 ? '1' : '0'), \
//         (byte & 0x04 ? '1' : '0'), \
//         (byte & 0x02 ? '1' : '0'), \
//         (byte & 0x01 ? '1' : '0')

//FIXME: NOT WORKING PROPERLY FOR CLIENT GET REQUEST
void *receive(int *socket, void *data, int dataLength)
{
    int n, x, receivedData = 0;
    data = malloc(2 * dataLength);
    char *datac = (char *)data;
    fprintf(stderr, "starting to read...\n");
    int readSize = 0;
    if (dataLength < 1024)
    {
        readSize = dataLength;
    }
    else if ((dataLength / 10) < 10480)
    {
        readSize = dataLength / 10;
    }
    else
    {
        readSize = 10480;
    }

    while (1)
    {
        if ((dataLength - receivedData) < readSize)
            readSize = dataLength - receivedData;
        n = read(*socket, datac + receivedData, dataLength - receivedData);
        // n = recv(*socket, datac + receivedData, readSize, 0);

        receivedData += n;
        // // fprintf(stderr, "read %d bytes!\n", n);

        if (n == 0 || receivedData == dataLength || n == -1)
            break;
    }

    if (n == -1 || receivedData != dataLength)
    {
        fprintf(stderr, "%s\n", strerror(errno));
        if (receivedData != dataLength)
        {
            fprintf(stderr, "number of Bytes is not enough to read the packet\n");
            // exit(1);
        }
        return NULL;
    }
    return data;

    int READLENGTH = 128;

    // int n, recieved;
    data = malloc(dataLength);
    // while (1)
    // {
    n = recv(*socket, data, dataLength, 0);
    // }

    if (n == -1)
    {
        fprintf(stderr, "%s/n", strerror(errno));
        // if (receivedData != dataLength)
        //     fprintf(stderr, "number of Bytes is not enough to read the packet");
        return NULL;
    }
    return data;
}

Info *recvInfo(int *socket)
{
    uint8_t *_info;
    _info = (uint8_t *)(receive(socket, _info, 1));
    if (_info != NULL)
    {
        Info *info = malloc(sizeof(Info));
        info->info = *_info;
        free(_info);
        return info;
    }
    free(_info);
    return NULL;
}
Control *recvControl(int *socket, Info *info)
{
    uint16_t *id;
    uint16_t *nodeId;
    uint32_t *nodeIp;
    uint16_t *nodePort;

    id = (uint16_t *)(receive(socket, id, 2));
    nodeId = (uint16_t *)(receive(socket, nodeId, 2));
    nodeIp = (uint32_t *)(receive(socket, nodeIp, 4));
    nodePort = (uint16_t *)(receive(socket, nodePort, 2));

    if (id != NULL && nodeId != NULL && nodeIp != NULL && nodePort != NULL)
    {
        Control *control = malloc(sizeof(Control));
        control->info = info->info;
        control->hashId = ntohs(*id);
        control->nodeId = ntohs(*nodeId);
        control->nodeIp = ntohl(*nodeIp);
        control->nodePort = ntohs(*nodePort);
        free(info);
        free(id);
        free(nodeId);
        free(nodeIp);
        free(nodePort);
        return control;
    }
    free(info);
    free(id);
    free(nodeId);
    free(nodeIp);
    free(nodePort);
    return NULL;
}

Header *rcvHeader(int *socket, Info *info)
{
    // uint8_t *info; // old
    uint16_t *keyLength;
    uint32_t *valueLength;
    // info = (uint8_t *)(receive(socket, info, 1));
    keyLength = (uint16_t *)(receive(socket, keyLength, 2));
    valueLength = (uint32_t *)(receive(socket, valueLength, 4));
    if (info != NULL && keyLength != NULL && valueLength != NULL)
    {
        Header *header = malloc(sizeof(Header));
        header->info = info->info;
        header->keyLength = ntohs(*keyLength);
        header->valueLength = ntohl(*valueLength);
        free(info);
        free(keyLength);
        free(valueLength);
        return header;
    }
    free(info);
    free(keyLength);
    free(valueLength);
    return NULL;
}
//if youre reading this guys, remind me to free my Mallocs
Body *readBody(int *socket, Header *header)
{
    Body *body = malloc(sizeof(Body));
    void *key = NULL;
    void *value = NULL; //(dont forget to free , if data nis not there)
    key = receive(socket, key, header->keyLength);
    fprintf(stderr, "%s\n", "about to read value...\n");

    body->key = key;
    // fprintf(stderr, "%s\n", "Value length: %" PRIu32 "\n", "!", header->valueLength);

    value = receive(socket, value, header->valueLength);
    fprintf(stderr, "%s\n", "error not in value...\n");
    body->value = value;
    return body;
}
void sendData(int *socket, void *data, int dataLength)
{
    // printf("%d bit of data to be sent!:\n", dataLength);
    int n = 0;
    int sentData = 0;
    char *datac = (char *)data; //cast every pointer to char pointer , to read the buffer byte after Byte
    // printf("Leading text " BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY((int)data), "\n");
    while (1)
    {
        n = send(*socket, datac + sentData, dataLength - sentData, 0);
        sentData += n;
        if (sentData == dataLength || sentData == -1)
        {
            break;
        }
    }
    if (n == -1)
    {
        fprintf(stderr, "%s/n", strerror(errno));
        // return -1;
    }
    //return sentData;
}

void printHeader(Header *header)
{
    fprintf(stderr, "\n\nHEADER:\nINFO: %" PRIu8 "\nKEYLENGTH: %" PRIu16 "\nVALUELENGTH: %" PRIu32 "\n",
            header->info, header->keyLength, header->valueLength);
}
