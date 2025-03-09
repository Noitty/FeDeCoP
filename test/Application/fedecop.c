#include "application.h"

typedef enum
{
    STANDBY,
    NEGOTIATION,
    CONSUMPTION,
    CLOSURE,
} StateProtocol;

int socket_fd;

Packet_fedecop rx_packet;
Packet_fedecop tx_packet;

int sat_destination;

int service_type;

char fed_buffer[BUFFER_SIZE]; /* Federation buffer for store the data send by the customer */
size_t fed_buffer_size;

char data_buffer[BUFFER_SIZE]; /* Buffer of data to be send to the provider */

bool waiting_Packet;

void start_fedecop(Satellite sat)
{
    /* Need to create the TCP socket here for both satelites */

    if (sat.available_service)
    {
        int server_fd, new_socket;
        struct sockaddr_in address;
        int addrlen = sizeof(address);

        // Creating socket file descriptor
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT_1);

        // Forcefully attaching socket to the port 8080
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }
        printf("Binded\n");

        if (listen(server_fd, 3) < 0)
        {
            perror("listen");
            exit(EXIT_FAILURE);
        }
        printf("Listening\n");

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        printf("Accepted connection with satellite. Start sending\n\n");
        socket_fd = new_socket;
    }
    else
    {
        int sock = 0, valread;
        struct sockaddr_in serv_addr;

        // Create socket file descriptor
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("\n Socket creation error \n");
            exit(EXIT_FAILURE);
        }

        memset(&serv_addr, '0', sizeof(serv_addr));

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT_1);

        // Convert IPv4 and IPv6 addresses from text to binary form
        if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
        {
            perror("\nInvalid address/ Address not supported \n");
            exit(EXIT_FAILURE);
        }

        sleep(5);
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            perror("\nConnection Failed \n");
            exit(EXIT_FAILURE);
        }
        printf("Connected with the publisher of the service\n");
        socket_fd = sock;
    }

    /* Variables */

    bool running = true;

    long time_tick;
    long spent_time;
    long time_to_sleep;
    long waiting_time_out = TIME_OUT;

    StateProtocol state = STANDBY;

    waiting_Packet = false;
    bool isRX;
    bool data_in_negotiation = false;

    /* Start */

    while (running)
    {
        /* Get current time to compute the rate */
        time_tick = get_current_time();

        if (checkReceivedPacket(sat, socket_fd, &running))
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
                /* Not done --> check the publisher capacity > 0 and that the current satellite have data to send */
                if (isRX == true && rx_packet.type == PACKET_SERVICE_REQUEST)
                {
                    printf("STANDBY PHASE: Received REQUEST Packet with service %d\n", rx_packet.service);
                    sat_destination = rx_packet.id_satellite;
                    sendAccept(sat, socket_fd);
                    printf("NEGOTIATION PHASE: ACCEPT Packet sent with service %d\n\n", rx_packet.service);
                    state = NEGOTIATION;
                    waiting_Packet = true;
                }
            }
            /* Customer */
            else
            {
                sendRequest(sat, socket_fd);
                printf("STANDBY PHASE: REQUEST Packet sent with service %d\n\n", sat.m_service_type_interest);
                state = NEGOTIATION;
                waiting_Packet = true;
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
                        sendData(sat, socket_fd);
                        printf("CONSUMPTION PHASE: DATA Packet sent\n\n");
                        waiting_Packet = true;
                        /* Set timers for Data_Ack, Close, Close_data_ack */
                        sleep(5);
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
                            sendCloseAckPacket(sat, socket_fd);
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
                                sendClosePacket(sat, socket_fd);
                                printf("CONSUMPTION PHASE: CLOSE Packet sent\n\n");
                                waiting_Packet = true;
                                /* Wait for the Close_Ack */
                            }
                            /* There is data to be send */
                            else
                            {
                                sendData(sat, socket_fd);
                                printf("CONSUMPTION PHASE: DATA Packet sent\n");
                                waiting_Packet = true;
                                /* Wait for Data_Ack || Close || Close_Data_Ack */
                            }
                            break;
                        case PACKET_CLOSE_DATA_ACK:
                            printf("CONSUMPTION PHASE: Received CLOSE DATA ACK Packet\n");
                            sendCloseAckPacket(sat, socket_fd);
                            state = CLOSURE;
                            printf("CONSUMPTION PHASE: CLOSE ACK Packet sent\n\n");
                            printf("\nThe FeDeCoP finished\n\n");
                            running = false;
                            close(socket_fd);
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
                            sendCloseAckPacket(sat, socket_fd);
                            printf("CLOSURE PHASE: CLOSE ACK Packet sent\n");
                            printf("\nThe FeDeCoP finished\n\n");
                            running = false;
                            close(socket_fd);
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
                            for(i = 0 ; i < BUFFER_SIZE ; i++)
                            {
                                if(fed_buffer[i] != '\0')
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
                                sendCloseDataAckPacket(sat, socket_fd);
                                printf("CONSUMPTION PHASE: CLOSE DATA ACK Packet sent\n");
                                waiting_Packet = true;
                                /* Wait for Close_Ack */
                                wait(5);
                            }
                            else
                            {
                                /* Keep sending more data */
                                sendDataAckPacket(sat, socket_fd);
                                printf("CONSUMPTION PHASE: DATA ACK Packet sent\n");
                                waiting_Packet = true;
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
                    if(rx_packet.type = PACKET_CLOSE_ACK)
                    {
                        printf("CLOSURE PHASE: Received CLOSE ACK Packet\n");
                        printf("The FeDeCoP finished\n\n");
                        state = STANDBY;
                        sat_destination = NON_DESTINATION;
                        running = false;
                        close(socket_fd);
                    }
                }
            }
            break;
        }

        /* Sleep */
        spent_time = get_current_time() + time_tick;
        if (spent_time < FEDECOP_PROTOCOL_SLEEP)
        {
            time_to_sleep = FEDECOP_PROTOCOL_SLEEP - spent_time;
            if (time_to_sleep <= FEDECOP_PROTOCOL_SLEEP && time_to_sleep > 0)
            {
                sleep(time_to_sleep);
            }
        }
    }
}

int checkReceivedPacket(Satellite sat, int socket_fd, bool *running)
{
    char buffer[sizeof(Packet_fedecop)];

    /* Change the defualt timeout */
    struct timeval tv;
    tv.tv_sec = 2; // Set timeout to 5 seconds
    tv.tv_usec = 0;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

    int bytes_received = recv(socket_fd, buffer, sizeof(buffer), 0);

    /* Put it to the default timeout again */
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, NULL, sizeof(struct timeval));

    // printf("%d\n", bytes_received);

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

void sendRequest(Satellite sat, int socket_fd)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.quantity = sizeof(fed_buffer);
    tx_packet.service = sat.m_service_type_interest;
    tx_packet.type = PACKET_SERVICE_REQUEST;
    memset(tx_packet.data, 0, sizeof(tx_packet.data));

    char buffer[sizeof(Packet_fedecop)];
    memcpy(buffer, &tx_packet, sizeof(Packet_fedecop));
    send(socket_fd, buffer, sizeof(buffer), 0);
}

void sendAccept(Satellite sat, int socket_fd)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.quantity = sizeof(fed_buffer);
    tx_packet.service = sat.type_of_service_available;
    tx_packet.type = PACKET_SERVICE_ACCEPT;
    memset(tx_packet.data, 0, sizeof(tx_packet.data));

    char buffer[sizeof(Packet_fedecop)];
    memcpy(buffer, &tx_packet, sizeof(Packet_fedecop));
    send(socket_fd, buffer, sizeof(buffer), 0);
}

void sendData(Satellite sat, int socket_fd)
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
    send(socket_fd, buffer, sizeof(buffer), 0);
}

void sendDataAckPacket(Satellite sat, int socket_fd)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.quantity = sizeof(fed_buffer);
    tx_packet.service = sat.type_of_service_available;
    tx_packet.type = PACKET_DATA_ACK;
    memset(tx_packet.data, 0, sizeof(tx_packet.data));

    char buffer[sizeof(Packet_fedecop)];
    memcpy(buffer, &tx_packet, sizeof(Packet_fedecop));
    send(socket_fd, buffer, sizeof(buffer), 0);
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
    send(socket_fd, buffer, sizeof(buffer), 0);
}

void sendCloseAckPacket(Satellite sat, int socket_fd)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.quantity = sizeof(fed_buffer);
    tx_packet.service = sat.type_of_service_available;
    tx_packet.type = PACKET_CLOSE_ACK;
    memset(tx_packet.data, 0, sizeof(tx_packet.data));

    char buffer[sizeof(Packet_fedecop)];
    memcpy(buffer, &tx_packet, sizeof(Packet_fedecop));
    send(socket_fd, buffer, sizeof(buffer), 0);
}

void sendCloseDataAckPacket(Satellite sat, int socket_fd)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.quantity = sizeof(fed_buffer);
    tx_packet.service = sat.type_of_service_available;
    tx_packet.type = PACKET_CLOSE_DATA_ACK;
    memset(tx_packet.data, 0, sizeof(tx_packet.data));

    char buffer[sizeof(Packet_fedecop)];
    memcpy(buffer, &tx_packet, sizeof(Packet_fedecop));
    send(socket_fd, buffer, sizeof(buffer), 0);
}

void reset_rx_packet_fedecop()
{
    rx_packet.protocol_number = -1;
    rx_packet.quantity = 0;
    rx_packet.service = SERVICE_TYPE_NOT_DEFINED;
    rx_packet.id_satellite = -1;
    rx_packet.type = 6; /* No valid type */
}

void call_start_fedecop(Satellite sat)
{
    start_fedecop(sat);
}