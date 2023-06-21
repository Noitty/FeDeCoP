#ifndef OSADP_H
#define OSADP_H

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT_1 8888

#define CUSTOMER 0
#define PROVIDER 1

#define NON_DESTINATION 0

/* Packet type */
#define PACKET_SERVICE_REQUEST 1
#define PACKET_SERVICE_ACCEPT 2
#define PACKET_DATA 4
#define PACKET_DATA_ACK 8
#define PACKET_CLOSE 16
#define PACKET_CLOSE_ACK 32
#define PACKET_CLOSE_DATA_ACK 64

#define SERVICE_TYPE_NOT_DEFINED -1
#define SERVICE_TYPE_STORAGE 0
#define SERVICE_TYPE_DOWNLOAD 1

#define BUFFER_SIZE 100

#define FEDECOP_PROT_NUMBER 2

#define TIME_OUT 15
#define PROTOCOL_SLEEP 0.5

typedef struct
{
    int type;   /* Accept, Request, ... */
    int id_satellite_source; /* id satellite that sends the packet */
    int service;    /* Type of service */
    int quantity;   /* How much of the service is requested */
    int protocol_number;
    unsigned char data[68]; /* For packet of 100 bytes max */

} Packet;

typedef struct
{
    int role; /* customer, provider */
    int type_of_service_available;
    int m_service_type_interest;
    int id_satellite;

} Satellite;

void start_fedecop(Satellite sat, int socket_fd);
int checkReceivedPacket(Satellite sat, int socket_fd, bool *running);
void sendRequest(Satellite sat, int socket_fd);
void sendAccept(Satellite sat, int socket_fd);
void sendData(Satellite sat, int socket_fd);
void sendDataAckPacket(Satellite sat, int socket_fd);
void sendClosePacket(Satellite sat, int socket_fd);
void sendCloseAckPacket(Satellite sat, int socket_fd);
void sendCloseDataAckPacket(Satellite sat, int socket_fd);
void reset_rx_packet();
long get_current_time();

#endif