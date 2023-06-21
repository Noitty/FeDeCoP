#include "application.h"

typedef enum
{
    STANDBY,
    NEGOTIATION,
    CONSUMPTION,
    CLOSURE,
} StateProtocol;

Packet_fedecop rx_packet;
Packet_fedecop tx_packet;

uint8_t sat_destination;

uint8_t service_type;

uint8_t fed_buffer[BUFFER_SIZE]; /* Federation buffer for store the data send by the customer */
size_t fed_buffer_size;

uint8_t data_buffer[BUFFER_SIZE]; /* Buffer of data to be send to the provider */

bool waiting_Packet;

void start_fedecop(Satellite sat, const struct Radio_s *radio)
{
    /* Variables */

    bool running = true;

    StateProtocol state = STANDBY;

    waiting_Packet = false;
    bool isRX;
    bool data_in_negotiation = false;

    /* Start */

    while (running)
    {
        if (checkReceivedPacket(sat, &running))
        {
            isRX = true;
        }
        else
        {
            isRX = false;
        }
        switch (state)
        {
        case STANDBY:
            /* Provider */
            if (sat.type_of_service_available != SERVICE_TYPE_NOT_DEFINED)
            {
                if (isRX == true && rx_packet.type == PACKET_SERVICE_REQUEST)
                {
                    printf("STANDBY PHASE: Received REQUEST Packet with service %d\n", rx_packet.service);
                    sat_destination = rx_packet.id_satellite;
                    sendAccept(sat);
                    radio->send(tx_packet, sizeof(tx_packet));
                    printf("NEGOTIATION PHASE: ACCEPT Packet sent with service %d\n\n", rx_packet.service);
                    state = NEGOTIATION;
                    waiting_Packet = true;
                    radio->RX(RX_TIMEOUT_VALUE);
                }
            }
            /* Customer */
            else
            {
                sendRequest(sat);
                radio->send(tx_packet, sizeof(tx_packet));
                printf("STANDBY PHASE: REQUEST Packet sent with service %d\n\n", sat.m_service_type_interest);
                state = NEGOTIATION;
                waiting_Packet = true;
                radio->RX(RX_TIMEOUT_VALUE);
            }
            break;
        case NEGOTIATION:
            if (isRX == true)
            {
                /* Customer receives the accept */
                if (waiting_Packet == true && rx_packet.type == PACKET_SERVICE_ACCEPT)
                {
                    waiting_Packet = false;
                    service_type = rx_packet.service;
                    printf("NEGOTIATION PHASE: Received ACCEPT Packet with service %d\n", service_type);
                    if (service_type == sat.m_service_type_interest)
                    {
                        state = CONSUMPTION;
                        sat.role = CUSTOMER;
                        printf("I am the CUSTOMER - Transit to CONSUMPTION PHASE\n");
                        sendData(sat);
                        radio->send(tx_packet, sizeof(tx_packet));
                        printf("CONSUMPTION PHASE: DATA Packet sent\n\n");
                        waiting_Packet = true;
                        radio->RX(RX_TIMEOUT_VALUE);
                        //sleep(5);
                    }
                }
                else if (waiting_Packet == true && rx_packet.type == PACKET_DATA)
                {
                    printf("NEGOTIATION PHASE: Received DATA Packet\n");
                    data_in_negotiation = true;
                    state = CONSUMPTION;
                    sat.role = PROVIDER;
                    printf("I am the PROVIDER - Transit to CONSUMPTION PHASE\n\n");
                    waiting_Packet = true;
                    radio->RX(RX_TIMEOUT_VALUE);
                }
            }
            break;
        case CONSUMPTION:
            if (sat.role == CUSTOMER)
            {
                if (isRX == true)
                {
                    printf("\nSomething received in the CUSTOMER with packet type %d\n", rx_packet.type);
                    if (waiting_Packet == true && (rx_packet.type == PACKET_DATA_ACK || rx_packet.type == PACKET_CLOSE || PACKET_CLOSE_DATA_ACK))
                    {
                        switch (rx_packet.type)
                        {
                        case PACKET_CLOSE:
                            printf("CONSUMPTION PHASE: Received CLOSE Packet\n");
                            state = CLOSURE;
                            sendCloseAckPacket(sat);
                            radio->send(tx_packet, sizeof(tx_packet));
                            printf("CONSUMPTION PHASE: CLOSE ACK Packet sent\n\n");
                            waiting_Packet = true;
                            /*
                             * Wait the close ACK
                             * Next iteration will be the closurePhase function
                             */
                            break;
                        case PACKET_DATA_ACK:
                            printf("CONSUMPTION PHASE: Received DATA ACK Packet\n");

                            /* NOT DONE --> Verify if I can keep sending, or if I have sent everything */
                            if (true)
                            {
                                printf("No more data in the payload buffer, I do not need the federation\n");
                                state = CLOSURE;
                                sendClosePacket(sat);
                                radio->send(tx_packet, sizeof(tx_packet));
                                printf("CONSUMPTION PHASE: CLOSE Packet sent\n\n");
                                waiting_Packet = true;
                                radio->RX(RX_TIMEOUT_VALUE);
                                /* Wait for the Close_Ack */
                            }
                            /* There is data to be send */
                            else
                            {
                                sendData(sat);
                                radio->send(tx_packet, sizeof(tx_packet));
                                printf("CONSUMPTION PHASE: DATA Packet sent\n");
                                waiting_Packet = true;
                                radio->RX(RX_TIMEOUT_VALUE);
                                /* Wait for Data_Ack || Close || Close_Data_Ack */
                            }
                            break;
                        case PACKET_CLOSE_DATA_ACK:
                            printf("CONSUMPTION PHASE: Received CLOSE DATA ACK Packet\n");
                            sendCloseAckPacket(sat);
                            radio->send(tx_packet, sizeof(tx_packet));
                            state = CLOSURE;
                            printf("CONSUMPTION PHASE: CLOSE ACK Packet sent\n\n");
                            printf("\nThe FeDeCoP finished\n\n");
                            running = false;
                            break;
                        }
                    }
                }
            }
            else if (sat.role == PROVIDER)
            {
                if (isRX == true || data_in_negotiation)
                {
                    printf("\nSomething received in the PROVIDER\n");
                    if (waiting_Packet == true && (rx_packet.type == PACKET_CLOSE || rx_packet.type == PACKET_DATA))
                    {
                        switch (rx_packet.type)
                        {
                        case PACKET_CLOSE:
                            printf("CONSUMPTION PHASE: Received CLOSE Packet\n");
                            state = CLOSURE;
                            sendCloseAckPacket(sat);
                            radio->send(tx_packet, sizeof(tx_packet));
                            printf("CLOSURE PHASE: CLOSE ACK Packet sent\n");
                            printf("\nThe FeDeCoP finished\n\n");
                            running = false;
                            break;
                        case PACKET_DATA:
                            printf("CONSUMPTION PHASE: Received DATA Packet\n");
                            /*
                             * Copy the content of the received paquet in the federation buffer
                             * and set the federation buffer size
                             */
                            memcpy(fed_buffer, rx_packet.data, sizeof(rx_packet.data));
                            /* Calculate the size of the fed_buffer */
                            int i, count = 0;
                            for (i = 0; i < BUFFER_SIZE; i++)
                            {
                                if (fed_buffer[i] != '\0')
                                {
                                    count++;
                                }
                            }
                            printf("CONSUMPTION PHASE: Packet stored in the buffer to be downloaded\n");

                            /* Notify the customer of the reception */
                            /* Check if my service is still available, fed buffer has space?*/
                            if (sat.type_of_service_available == SERVICE_TYPE_NOT_DEFINED || count == BUFFER_SIZE)
                            {
                                /*
                                 * The download slot is over, then I have to close the federation.
                                 * Or my buffer is full. I do not have the service any more.
                                 */
                                printf("CONSUMPTION PHASE: Service no longer available\n");
                                state = CLOSURE;
                                sendCloseDataAckPacket(sat);
                                radio->send(tx_packet, sizeof(tx_packet));
                                printf("CONSUMPTION PHASE: CLOSE DATA ACK Packet sent\n");
                                waiting_Packet = true;
                                radio->RX(RX_TIMEOUT_VALUE);
                                /* Wait for Close_Ack */
                                // wait(5);
                            }
                            else
                            {
                                /* Keep sending more data */
                                sendDataAckPacket(sat);
                                radio->send(tx_packet, sizeof(tx_packet));
                                printf("CONSUMPTION PHASE: DATA ACK Packet sent\n");
                                waiting_Packet = true;
                                radio->RX(RX_TIMEOUT_VALUE);
                                /* Wait for more data */
                            }
                            break;
                        }
                    }
                    data_in_negotiation = false;
                }
            }
            break;
        case CLOSURE:
            if (isRX == true)
            {
                if (waiting_Packet == true && (rx_packet.type == PACKET_CLOSE_ACK || rx_packet.type == PACKET_CLOSE))
                {
                    if (rx_packet.type = PACKET_CLOSE_ACK)
                    {
                        printf("CLOSURE PHASE: Received CLOSE ACK Packet\n");
                        printf("The FeDeCoP finished\n\n");
                        state = STANDBY;
                        sat_destination = NON_DESTINATION;
                        running = false;
                        radio->Standby();
                    }
                }
            }
            break;
        }
    }
}

int checkReceivedPacket(Satellite sat, bool *running)
{
    if (bytes_received > 0)
    {
        Packet_fedecop aux;
        memcpy(&aux, buffer, sizeof(Packet_fedecop));

        if (aux.id_satellite == sat.id_satellite)
        {
            return -1;
        }
        if (aux.protocol_number == FEDECOP_PROT_NUMBER)
        {
            reset_rx_packet_fedecop();
            rx_packet = aux;
            return bytes_received;
        }
        else
        {
            printf("\nReceived packet is not from the FeDeCoP PROTOCOL\n");
            *running = false;
        }
    }
}

void sendRequest(Satellite sat)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.quantity = sizeof(fed_buffer);
    tx_packet.service = sat.m_service_type_interest;
    tx_packet.type = PACKET_SERVICE_REQUEST;
    memset(tx_packet.data, 0, sizeof(tx_packet.data));

    char buffer[sizeof(Packet_fedecop)];
    memcpy(buffer, &tx_packet, sizeof(Packet_fedecop));
}

void sendAccept(Satellite sat)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.quantity = sizeof(fed_buffer);
    tx_packet.service = sat.type_of_service_available;
    tx_packet.type = PACKET_SERVICE_ACCEPT;
    memset(tx_packet.data, 0, sizeof(tx_packet.data));

    char buffer[sizeof(Packet_fedecop)];
    memcpy(buffer, &tx_packet, sizeof(Packet_fedecop));
}

void sendData(Satellite sat)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.quantity = sizeof(fed_buffer);
    tx_packet.service = sat.type_of_service_available;
    tx_packet.type = PACKET_DATA;
    memcpy(tx_packet.data, data_buffer, 68);
    memmove(data_buffer, data_buffer + 68, 32);

    char buffer[sizeof(Packet_fedecop)];
    memcpy(buffer, &tx_packet, sizeof(Packet_fedecop));
}

void sendDataAckPacket(Satellite sat)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.quantity = sizeof(fed_buffer);
    tx_packet.service = sat.type_of_service_available;
    tx_packet.type = PACKET_DATA_ACK;
    memset(tx_packet.data, 0, sizeof(tx_packet.data));

    char buffer[sizeof(Packet_fedecop)];
    memcpy(buffer, &tx_packet, sizeof(Packet_fedecop));
}

void sendClosePacket(Satellite sat, int socket_fd)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.quantity = sizeof(fed_buffer);
    tx_packet.service = sat.type_of_service_available;
    tx_packet.type = PACKET_CLOSE;
    memset(tx_packet.data, 0, sizeof(tx_packet.data));

    char buffer[sizeof(Packet_fedecop)];
    memcpy(buffer, &tx_packet, sizeof(Packet_fedecop));
}

void sendCloseAckPacket(Satellite sat)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.quantity = sizeof(fed_buffer);
    tx_packet.service = sat.type_of_service_available;
    tx_packet.type = PACKET_CLOSE_ACK;
    memset(tx_packet.data, 0, sizeof(tx_packet.data));

    char buffer[sizeof(Packet_fedecop)];
    memcpy(buffer, &tx_packet, sizeof(Packet_fedecop));
}

void sendCloseDataAckPacket(Satellite sat)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.quantity = sizeof(fed_buffer);
    tx_packet.service = sat.type_of_service_available;
    tx_packet.type = PACKET_CLOSE_DATA_ACK;
    memset(tx_packet.data, 0, sizeof(tx_packet.data));

    char buffer[sizeof(Packet_fedecop)];
    memcpy(buffer, &tx_packet, sizeof(Packet_fedecop));
}

void reset_rx_packet_fedecop()
{
    rx_packet.protocol_number = -1;
    rx_packet.quantity = 0;
    rx_packet.service = SERVICE_TYPE_NOT_DEFINED;
    rx_packet.id_satellite = -1;
    rx_packet.type = 6; /* No valid type */
}