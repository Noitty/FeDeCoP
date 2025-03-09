#include "application.h"

Packet_osadp rx_packet;
Packet_osadp tx_packet;

struct sockaddr_in server_addr, client_addr, client2_addr, publish_addr, received_addr;
int send_request_adress_length;

/* ************************************* */
/* SERVICE LIST VARIABLES */
/* Linked list */

struct node
{
    int data;
    int key;
    struct node *next;
};

struct node *head = NULL;
struct node *current = NULL;

/* ************************************* */

void start_osadp(Satellite sat, int port)
{
    /* Variables */
    long spent_time;
    long time_tick;
    long time_to_sleep;
    long service_expiration_date = get_current_time() + SERVICE_LIFETIME;
    long creation_date_publish_packet;

    long publishing_next = 0;

    bool cont = true;

    /* Create and set the socket */
    int sock;
    int client_adrr_len = sizeof(client_addr);
    int client2_adrr_len = sizeof(client2_addr);
    int publish_addr_len = sizeof(publish_addr);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully\n");

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));
    memset(&client2_addr, 0, sizeof(client2_addr));
    memset(&publish_addr, 0, sizeof(publish_addr));
    memset(&received_addr, 0, sizeof(received_addr));

    if (port == PORT_1)
    {
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = INADDR_ANY;
        client2_addr.sin_family = AF_INET;
        client2_addr.sin_addr.s_addr = INADDR_ANY;
        client_addr.sin_port = htons(PORT_2);
        client2_addr.sin_port = htons(7777);
    }
    else if (port == PORT_2)
    {
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = INADDR_ANY;
        client2_addr.sin_family = AF_INET;
        client2_addr.sin_addr.s_addr = INADDR_ANY;
        client_addr.sin_port = htons(PORT_1);
        client2_addr.sin_port = htons(PORT_3);
    }
    else
    {
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = INADDR_ANY;
        client2_addr.sin_family = AF_INET;
        client2_addr.sin_addr.s_addr = INADDR_ANY;
        client_addr.sin_port = htons(PORT_1);
        client2_addr.sin_port = htons(7778);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sock, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    /* ************************************** */

    while (cont)
    {
        /* Get current time to compute the rate */
        time_tick = get_current_time();

        /* Check if the satellite has an available service */
        if (sat.available_service > 0)
        {
            /* Check the transmission date and the service expiration date */
            if ((get_current_time() >= publishing_next || publishing_next == 0) && service_expiration_date > get_current_time())
            {
                /* Have to be broadcast!! */
                send_publish(sock, sat);
                printf("\nPublish packet sent\n");
                publishing_next = get_current_time() + PUBLISHING_PERIOD; /* Period of 10 seconds */
            }
        }

        /* Check if received packet, if yes --> check it is a packet from the OSADP_PROTOCOL (is a publish) */
        if (check_received_packet(sock, &cont, sat) > 0)
        {
            if (rx_packet.type == PACKET_START && rx_packet.protocol_number == FEDECOP_PROT_NUMBER && sat.available_service)
            {
                printf("\nOne satellite is interested\n");
                printf("Start FeDeCoP\n\n");
                start_fedecop(sat);
            }

            /* An else if for forwarding the start packet if its not for that satellite */
            /* else if (rx_packet.type == PACKET_START && rx_packet.protocol_number == FEDECOP_PROT_NUMBER && !sat.available_service)
            {
                send the packet to the publish_addr (to the satellite that send the publish to me)
            } */

            else
            {
                printf("\nPublish packet received from satellite id: %d with service from satellite %d\n", rx_packet.id_satellite, rx_packet.id_satellite_service);

                /*
                 * Check if:
                 * (The actual time is lower than the expiration date of the service) && (The service of the rx_packet is not in the service table)
                 * If not --> discart
                 */

                if ((get_current_time() < service_expiration_date) && (find(rx_packet.id_satellite) == NULL))
                {
                    /* Add the service and the id of the satellite in the service table */
                    insertFirst(rx_packet.id_satellite_service, rx_packet.service);
                    printf("\nThe service in the publish is not in the service table, we add it\n");
                    printf("\nServices in the service table [id_satellite,service_type]:");
                    printList();

                    /* We save the address of the publisher in case we act as a link for the request packet */
                    publish_addr = received_addr;
                    //printf("IP: %s and port: %i\n", inet_ntoa(publish_addr.sin_addr), ntohs(publish_addr.sin_port));

                    if (rx_packet.service == sat.m_service_type_interest)
                    {
                        printf("\nI am interested in the service\n");
                        printf("Implement FeDeCoP\n\n");
                        send_start(sat, sock);
                        /*
                         * UDP socket is closed
                         */
                        //close(sock);
                        start_fedecop(sat);
                    }
                    else if (rx_packet.service != sat.m_service_type_interest)
                    {
                        printf("\nNot interested in the service of the satellite id: %d\n", rx_packet.id_satellite);
                        rx_packet.hop_limit--;
                        if (rx_packet.hop_limit > 0)
                        {
                            /* Have to be broadcast!! */
                            forward_publish(sock, sat);
                            printf("Forward publish packet\n");
                        }
                        else
                        {
                            printf("The number of hop limit is 0, the packet is discarted\n");
                        }
                    }
                }
                else if (get_current_time() >= service_expiration_date)
                {
                    printf("\nThe service has expired\n");
                }
                else
                {
                    printf("\nThe publish packet is already in the service table, it is discarted\n");
                }
            }

            printf("\n**********************\n");
        }

        /* Sleep */
        spent_time = get_current_time() - time_tick;
        if (spent_time < OSADP_PROTOCOL_SLEEP)
        {
            time_to_sleep = OSADP_PROTOCOL_SLEEP - spent_time;
            if (time_to_sleep <= OSADP_PROTOCOL_SLEEP && time_to_sleep > 0)
            {
                sleep(time_to_sleep);
            }
        }
        else
        {
            // printf("No sleep, the process consumed %ld\n", time_tick);
        }
    }

    close(sock);
}

/* Stores the recived packet in rx_packet 
 * Check the received packet has the OSADP_PROT_NUMBER 
 */

int check_received_packet(int sock, bool *cont, Satellite sat)
{
    char buffer[sizeof(Packet_osadp)];

    /* Change the defualt timeout */
    struct timeval tv;
    tv.tv_sec = 2; // Set timeout to 5 seconds
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

    /* Should store the address of the received packet in send_request_adress_length (NOT DOING IT) */
    int n = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&received_addr, NULL);

    /* Put it to the default timeout again */
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, NULL, sizeof(struct timeval));

    if (n > 0)
    {
        Packet_osadp aux;
        memcpy(&aux, buffer, sizeof(Packet_osadp));

        if (aux.id_satellite == sat.id_satellite || (aux.protocol_number == OSADP_PROT_NUMBER && aux.id_satellite_service == sat.id_satellite))
        {
            return -1;
        }
        if (aux.protocol_number == OSADP_PROT_NUMBER)
        {
            reset_rx_packet_osadp();
            rx_packet = aux;
            
            return n;
        }
        else if (aux.protocol_number == FEDECOP_PROT_NUMBER)
        {
            reset_rx_packet_osadp();
            rx_packet = aux;
            return n;
        }
        else
        {
            printf("\nReceived packet is not from the OSADP PROTOCOL\n");
            *cont = false;
        }
    }
    return -1;
}

void send_publish(int sock, Satellite sat)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.creation_date = get_current_time();
    tx_packet.hop_limit = HOP_LIMIT;
    tx_packet.protocol_number = OSADP_PROT_NUMBER;
    tx_packet.service = sat.type_of_service_available;
    tx_packet.id_satellite_service = sat.id_satellite;
    tx_packet.type = PACKET_SERVICE_PUBLISH;

    char buffer[sizeof(Packet_osadp)];
    memcpy(buffer, &tx_packet, sizeof(Packet_osadp));

    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
    {
        perror("Error setting socket options");
        exit(1);
    }
    client_addr.sin_addr.s_addr = INADDR_BROADCAST;
    client2_addr.sin_addr.s_addr = INADDR_BROADCAST;

    sendto(sock, buffer, sizeof(buffer), 0, &client_addr, sizeof(client_addr));
    // sendto(sock, buffer, sizeof(buffer), 0, &client2_addr, sizeof(client2_addr));
}

void send_start(Satellite sat, int sock)
{
    tx_packet.id_satellite = sat.id_satellite;
    tx_packet.creation_date = get_current_time();
    tx_packet.hop_limit = HOP_LIMIT;
    tx_packet.protocol_number = FEDECOP_PROT_NUMBER;
    tx_packet.service = sat.type_of_service_available;
    tx_packet.id_satellite_service = rx_packet.id_satellite_service;
    tx_packet.type = PACKET_START;

    char buffer[sizeof(Packet_osadp)];
    memcpy(buffer, &tx_packet, sizeof(Packet_osadp));

    sendto(sock, buffer, sizeof(buffer), 0, &client_addr, sizeof(client_addr));
}

void forward_publish(int sock, Satellite sat)
{
    rx_packet.id_satellite = sat.id_satellite;
    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
    {
        perror("Error setting socket options");
        exit(1);
    }
    client_addr.sin_addr.s_addr = INADDR_BROADCAST;
    client2_addr.sin_addr.s_addr = INADDR_BROADCAST;

    char buffer[sizeof(Packet_osadp)];
    memcpy(buffer, &rx_packet, sizeof(Packet_osadp));

    sendto(sock, buffer, sizeof(buffer), 0, &client_addr, sizeof(client_addr));
    sendto(sock, buffer, sizeof(buffer), 0, &client2_addr, sizeof(client2_addr));
}

long get_current_time()
{
    time_t actual_time = time(NULL);
    return ((long)actual_time);
}

void reset_rx_packet_osadp()
{
    rx_packet.creation_date = -1;
    rx_packet.hop_limit = -1;
    rx_packet.id_satellite = -1;
    rx_packet.protocol_number = -1;
    rx_packet.service = SERVICE_TYPE_NOT_DEFINED;
    rx_packet.type = -1;
}

/* ************************************************ */
/* SERVICE LIST */

// display the list
void printList()
{
    struct node *ptr = head;
    printf("\n[ ");

    // start from the beginning
    while (ptr != NULL)
    {
        if (ptr->data == SERVICE_TYPE_STORAGE)
        {
            printf("(%d,SERVICE_TYPE_STORAGE) ", ptr->key);
            ptr = ptr->next;
        }
        else if (ptr->data == SERVICE_TYPE_DOWNLOAD)
        {
            printf("(%d,SERVICE_TYPE_DOWNLOAD) ", ptr->key, ptr->data);
            ptr = ptr->next;
        }
    }

    printf(" ]\n");
}

// insert link at the first location
void insertFirst(int key, int data)
{
    // create a link
    struct node *link = (struct node *)malloc(sizeof(struct node));

    link->key = key;
    link->data = data;

    // point it to old first node
    link->next = head;

    // point first to new first node
    head = link;
}

// delete first item
struct node *deleteFirst()
{

    // save reference to first link
    struct node *tempLink = head;

    // mark next to first link as first
    head = head->next;

    // return the deleted link
    return tempLink;
}

// is list empty
bool isEmpty()
{
    return head == NULL;
}

int length()
{
    int length = 0;
    struct node *current;

    for (current = head; current != NULL; current = current->next)
    {
        length++;
    }

    return length;
}

// find a link with given key
struct node *find(int key)
{

    // start from the first link
    struct node *current = head;

    // if list is empty
    if (head == NULL)
    {
        return NULL;
    }

    // navigate through list
    while (current->key != key)
    {

        // if it is last node
        if (current->next == NULL)
        {
            return NULL;
        }
        else
        {
            // go to next link
            current = current->next;
        }
    }

    // if data found, return the current Link
    return current;
}

// delete a link with given key
struct node *delete(int key)
{

    // start from the first link
    struct node *current = head;
    struct node *previous = NULL;

    // if list is empty
    if (head == NULL)
    {
        return NULL;
    }

    // navigate through list
    while (current->key != key)
    {

        // if it is last node
        if (current->next == NULL)
        {
            return NULL;
        }
        else
        {
            // store reference to current link
            previous = current;
            // move to next link
            current = current->next;
        }
    }

    // found a match, update the link
    if (current == head)
    {
        // change first to point to next link
        head = head->next;
    }
    else
    {
        // bypass the current link
        previous->next = current->next;
    }

    return current;
}

void sort()
{

    int i, j, k, tempKey, tempData;
    struct node *current;
    struct node *next;

    int size = length();
    k = size;

    for (i = 0; i < size - 1; i++, k--)
    {
        current = head;
        next = head->next;

        for (j = 1; j < k; j++)
        {

            if (current->data > next->data)
            {
                tempData = current->data;
                current->data = next->data;
                next->data = tempData;

                tempKey = current->key;
                current->key = next->key;
                next->key = tempKey;
            }

            current = current->next;
            next = next->next;
        }
    }
}

void reverse(struct node **head_ref)
{
    struct node *prev = NULL;
    struct node *current = *head_ref;
    struct node *next;

    while (current != NULL)
    {
        next = current->next;
        current->next = prev;
        prev = current;
        current = next;
    }

    *head_ref = prev;
}
