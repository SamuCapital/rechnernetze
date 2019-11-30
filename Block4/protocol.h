
#ifndef UNTITLED_PROTOCOL_H
#define UNTITLED_PROTOCOL_H

typedef struct _info
{
    uint8_t info;
} Info;

typedef struct _header
{
    // Info info;
    uint8_t info; //old
    uint16_t keyLength;
    uint32_t valueLength;
} Header;
typedef struct _body
{
    void *key;
    void *value;
} Body;

typedef struct _Control
{
    uint8_t info;
    uint16_t hashId;
    uint16_t nodeId;
    uint32_t nodeIp;
    uint16_t nodePort;

} Control;

void *receive(int *socket, void *data, int dataLength);
Info *recvInfo(int *socket);
Control *recvControl(int *socket, Info *info);
Header *rcvHeader(int *socket, Info *info);
Body *readBody(int *socket, Header *header);
void sendData(int *socket, void *data, int dataLength);
void printHeader(Header *header);
void printControl(Control *control);
void printControlDetails(uint32_t ip, uint16_t port);
int endian();

#endif //UNTITLED_PROTOCOL_H
