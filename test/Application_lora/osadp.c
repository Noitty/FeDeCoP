#include "application.h"

Packet_osadp rx_packet;
Packet_osadp tx_packet;

uint8_t Buffer[BUFFER_SIZE];

bool PacketReceived = false;
bool inFedecop = false;

/********  LoRa PARAMETERS  ********/
uint8_t SF = LORA_SPREADING_FACTOR; // Spreading Factor
uint8_t CR = LORA_CODINGRATE;       // Coding Rate
uint16_t time_packets = 500;        // Time between data packets sent in ms

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

void configuration(Radio_s *radio)
{
    /* Reads the SF, CR and time between packets variables from memory */
    SF = LORA_SPREADING_FACTOR;
    CR = LORA_CODINGRATE;
    time_packets = 500;

    /* Configuration of the LoRa frequency and TX and RX parameters */
    radio->SetChannel(RF_FREQUENCY);

    radio->SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH, SF, CR,
                       LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                       true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

    radio->SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, SF, CR, 0, LORA_PREAMBLE_LENGTH,
                       LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                       0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
};

void start_osadp(Satellite sat)
{
    /* Target board initialization*/
    BoardInitMcu();
    BoardInitPeriph();

    /* Radio initialization */
    RadioEvents.TxDone = OnTxDone; // standby
    RadioEvents.RxDone = OnRxDone; // standby
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;

    Radio.Init(&RadioEvents); // Initializes the Radio

    configuration(&Radio); // Configures the transceiver

    xEventGroupSetBits(xEventGroup, COMMS_DONE_EVENT);

    /* Variables */
    long service_expiration_date = get_current_time() + SERVICE_LIFETIME;
    long publishing_next = 0;
    bool cont = true;

    /* ************************************** */

    while (cont)
    {
        /* Check if the satellite has an available service */
        if (sat.available_service > 0)
        {
            /* Check the transmission date and the service expiration date */
            if ((get_current_time() >= publishing_next || publishing_next == 0) && service_expiration_date > get_current_time())
            {
                /* Have to be broadcast!! */
                send_publish(sat);
                printf("\nPublish packet sent\n");
                publishing_next = get_current_time() + PUBLISHING_PERIOD; /* Period of 10 seconds */
            }
        }

        /* Check if received packet, if yes --> check it is a packet from the OSADP_PROTOCOL (is a publish) OR FEDECOP (a request)*/
        if (PacketReceived)
        {
            if (rx_packet.type == PACKET_SERVICE_REQUEST && rx_packet.protocol_number == FEDECOP_PROT_NUMBER && sat.available_service)
            {
                printf("\nOne satellite is interested\n");
                printf("Implement FeDeCoP\n\n");
                inFedecop = true;
                start_fedecop(sat, &Radio);
                inFedecop = false;
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
                    // printf("IP: %s and port: %i\n", inet_ntoa(publish_addr.sin_addr), ntohs(publish_addr.sin_port));

                    if (rx_packet.service == sat.m_service_type_interest)
                    {
                        printf("\nI am interested in the service\n");
                        printf("Implement FeDeCoP\n\n");
                        inFedecop = true;
                        start_fedecop(sat, &Radio);
                        inFedecop = false;
                    }
                    else if (rx_packet.service != sat.m_service_type_interest)
                    {
                        printf("\nNot interested in the service of the satellite id: %d\n", rx_packet.id_satellite);
                        rx_packet.hop_limit--;
                        if (rx_packet.hop_limit > 0)
                        {
                            /* Have to be broadcast!! */
                            forward_publish(sat);
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

            Radio.Standby();
        }

        else
        {
            Radio.RX(RX_TIMEOUT_VALUE);
        }
    }
}

/* Stores the recived packet in rx_packet
 * Check the received packet has the OSADP_PROT_NUMBER
 */

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    Radio.Standby();

    memset(Buffer, 0, size);

    memcpy(Buffer, payload, size);

    if (!inFedecop)
    {
        memcpy(&aux, Buffer, sizeof(Packet_osadp));

        if (sizeof(Buffer) != 0)
        {
            if (aux.id_satellite == sat.id_satellite || (aux.protocol_number == OSADP_PROT_NUMBER && aux.id_satellite_service == sat.id_satellite))
            {
                return -1;
            }
            if (aux.protocol_number == OSADP_PROT_NUMBER)
            {
                reset_rx_packet_osadp();
                rx_packet = aux;
                PacketReceived = true;
                return n;
            }
            else if (aux.protocol_number == FEDECOP_PROT_NUMBER && aux.type == PACKET_SERVICE_REQUEST)
            {
                reset_rx_packet_osadp();
                rx_packet = aux;
                PacketReceived = true;
                return n;
            }
            else
            {
                printf("\nReceived packet is not from the OSADP PROTOCOL\n");
                *cont = false;
            }
        }
    }
    else
    {
        memcpy(&aux, Buffer, sizeof(Packet_fedecop));
        if (sizeof(Buffer) != 0)
        {
            rx_packet = aux;
        }
    }
}

void send_publish(Satellite sat)
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

    Radio.Send(buffer, sizeof(buffer));
}

void forward_publish(Satellite sat)
{
    rx_packet.id_satellite = sat.id_satellite;

    char buffer[sizeof(Packet_osadp)];
    memcpy(buffer, &rx_packet, sizeof(Packet_osadp));

    Radio.Send(buffer, sizeof(buffer));
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

void OnTxDone(void)
{
    Radio.Rx(RX_TIMEOUT_VALUE);
}

void OnTxTimeout(void)
{
    // Radio.Standby( );
    Radio.Rx(RX_TIMEOUT_VALUE);
}

void OnRxTimeout(void)
{
    Radio.Rx(RX_TIMEOUT_VALUE);
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