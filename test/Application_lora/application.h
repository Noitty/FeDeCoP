#ifndef APPLICATION_H
#define APPLICATION_H

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "sx126x.h"
#include "radio.h"
#include "sx126x-board.h"

#include "flash.h"

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include "notifications.h"
#include "correct.h"
#include "ecc.h"

#define RF_FREQUENCY                       	868000000 // Hz

#define TX_OUTPUT_POWER                     22        // dBm

#define LORA_BANDWIDTH                      0         // [0: 125 kHz,
													  //  1: 250 kHz,
													  //  2: 500 kHz,
													  //  3: Reserved]
#define LORA_SPREADING_FACTOR               11         // [SF7..SF12]
#define LORA_CODINGRATE                     1         // [1: 4/5,
													  //  2: 4/6,
													  //  3: 4/7,
													  //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                8//108    // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                 100       // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON          false
#define LORA_IQ_INVERSION_ON                false
#define LORA_FIX_LENGTH_PAYLOAD_LEN         19
#define WINDOW_SIZE							20

#define RX_TIMEOUT_VALUE                    4000
#define BUFFER_SIZE							100

#define TLE_PACKET_SIZE						66
#define TELEMETRY_PACKET_SIZE				34
#define CALIBRATION_PACKET_SIZE				96
#define CONFIG_PACKET_SIZE					30
#define DATA_PACKET_SIZE                    96

/* Satellite struct */
typedef struct
{
    uint8_t role; /* customer, provider */
    bool available_service;
    uint8_t type_of_service_available;
    uint8_t m_service_type_interest;
    uint8_t id_satellite;

} Satellite;

/* OSADP Packet struct */
typedef struct
{
    uint8_t id_satellite;         /* id of the satellite that send the packet */
    uint8_t id_satellite_service; /* id of the satellite that send the service  */
    uint8_t service;
    uint8_t protocol_number;
    uint8_t hop_limit;
    uint8_t creation_date;
    uint8_t type;
} Packet_osadp;

/* FeDeCoP packet struct */
typedef struct
{
    uint8_t type;                /* Accept, Request, ... */
    uint8_t id_satellite; /* id satellite that sends the packet */
    uint8_t service;             /* Type of service */
    uint8_t quantity;            /* How much of the service is requested */
    uint8_t protocol_number;
    uint8_t data[60]; /* For packet of 100 bytes max */

} Packet_fedecop;

/* State of service */
#define SERVICE_TYPE_NOT_DEFINED -1
#define SERVICE_TYPE_STORAGE 0
#define SERVICE_TYPE_DOWNLOAD 1

/* Packet type */
#define PACKET_SERVICE_PUBLISH 1
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
void start_osadp(Satellite sat);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
void send_publish(Satellite sat);
void forward_publish(Satellite sat);
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxTimeout(void);
long get_current_time();
void reset_rx_packet_osadp();
void printList();
void insertFirst(int key, int data);
struct node *deleteFirst();
struct node *find(int key);

/* FeDeCoP functions */
void start_fedecop(Satellite sat);
int checkReceivedPacket(Satellite sat, bool *running);
void sendRequest(Satellite sat);
void sendAccept(Satellite sat);
void sendData(Satellite sat);
void sendDataAckPacket(Satellite sat);
void sendClosePacket(Satellite sat);
void sendCloseAckPacket(Satellite sat);
void sendCloseDataAckPacket(Satellite sat);
void reset_rx_packet_fedecop();

#endif