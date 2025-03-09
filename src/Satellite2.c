#include "fedecop.h"

int main(){
    Satellite sat;
    sat.type_of_service_available = SERVICE_TYPE_NOT_DEFINED;
    sat.m_service_type_interest = SERVICE_TYPE_STORAGE;
    sat.id_satellite = 2;

    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    
    // Create socket file descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT_1);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    printf("Connected to the server\n");

    start_fedecop(sat, sock);
}