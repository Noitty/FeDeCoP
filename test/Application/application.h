#ifndef APPLICATION_H
#define APPLICATION_H

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* Satellite struct */
typedef struct
{
    int role; /* customer, provider */
    bool available_service;
    int type_of_service_available;
    int m_service_type_interest;
    int id_satellite;

} Satellite;

/* OSADP Packet struct */
typedef struct
{
    int id_satellite;         /* id of the satellite that send the packet */
    int id_satellite_service; /* id of the satellite that send the service  */
    int service;
    int protocol_number;
    int hop_limit;
    long creation_date;
    int type;
} Packet_osadp;

/* FeDeCoP packet struct */
typedef struct
{
    int type;                /* Accept, Request, ... */
    int id_satellite; /* id satellite that sends the packet */
    int service;             /* Type of service */
    int quantity;            /* How much of the service is requested */
    int protocol_number;
    unsigned char data[68]; /* For packet of 100 bytes max */

} Packet_fedecop;

typedef struct
{
    void *fn;
} event;

/* Ports */
#define PORT_1 8888
#define PORT_2 8889
#define PORT_3 8880

/* State of service */
#define SERVICE_TYPE_NOT_DEFINED -1
#define SERVICE_TYPE_STORAGE 0
#define SERVICE_TYPE_DOWNLOAD 1

/* Packet type */
#define PACKET_SERVICE_PUBLISH 1
#define PACKET_START 2
#define PACKET_SERVICE_REQUEST 4
#define PACKET_SERVICE_ACCEPT 8
#define PACKET_DATA 16
#define PACKET_DATA_ACK 32
#define PACKET_CLOSE 64
#define PACKET_CLOSE_ACK 128
#define PACKET_CLOSE_DATA_ACK 256
#define PACKET_SERVICE_PUBLISH 512

/* Buffer */
#define BUFFER_SIZE 100

/* Protocols number */
#define OSADP_PROT_NUMBER 1
#define FEDECOP_PROT_NUMBER 2

/* OSADP constants */
#define PUBLISHING_PERIOD 10
#define OSADP_PROTOCOL_SLEEP 0.5
#define HOP_LIMIT 5
#define SERVICE_LIFETIME 600

/* FeDeCoP constants */
#define TIME_OUT 15
#define FEDECOP_PROTOCOL_SLEEP 0.5
#define CUSTOMER 0
#define PROVIDER 1
#define NON_DESTINATION 0

/* OSADP functions */
void start_osadp(Satellite sat, int port);
int check_received_packet(int sock, bool *cont, Satellite sat);
void send_publish(int sock, Satellite sat);
void send_start(Satellite sat, int sock);
void forward_publish(int sock, Satellite sat);
long get_current_time();
void reset_rx_packet_osadp();
void printList();
void insertFirst(int key, int data);
struct node *deleteFirst();
struct node *find(int key);

/* FeDeCoP functions */
void start_fedecop(Satellite sat);
int checkReceivedPacket(Satellite sat, int socket_fd, bool *running);
void sendRequest(Satellite sat, int socket_fd);
void sendAccept(Satellite sat, int socket_fd);
void sendData(Satellite sat, int socket_fd);
void sendDataAckPacket(Satellite sat, int socket_fd);
void sendClosePacket(Satellite sat, int socket_fd);
void sendCloseAckPacket(Satellite sat, int socket_fd);
void sendCloseDataAckPacket(Satellite sat, int socket_fd);
void reset_rx_packet_fedecop();

#endif