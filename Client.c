/* Updated UDP Client Implementation - Assignment 1 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib") // Link with Winsock library

#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"
#define MAX_PAYLOAD_SIZE 255
#define MAX_PACKET_SIZE (2 + 1 + 2 + MAX_PAYLOAD_SIZE + 2)

// Packet identifiers
#define START_PACKET_ID 0xFFFF
#define END_PACKET_ID 0xFFFF

// Packet types
#define DATA_PACKET 0xFFF1
#define ACK_PACKET 0xFFF2
#define REJECT_PACKET 0xFFF3

// Reject sub-codes
#define REJECT_OUT_OF_SEQUENCE 0xFFF4
#define REJECT_LENGTH_MISMATCH 0xFFF5
#define REJECT_END_MISSING 0xFFF6
#define REJECT_DUPLICATE_PACKET 0xFFF7

// Timer settings
#define ACK_TIMEOUT 3000 // in milliseconds
#define MAX_RETRIES 3

// Packet structure for sending data
typedef struct {
    unsigned short start_id;
    unsigned char client_id;
    unsigned short type;
    unsigned short segment_no;
    unsigned char length;
    char payload[MAX_PAYLOAD_SIZE];
    unsigned short end_id;
} DataPacket;

void send_data_packets(SOCKET sockfd, struct sockaddr_in *server_addr);
int receive_response(SOCKET sockfd, unsigned short expected_segment_no);
void simulate_errors(DataPacket *packet, int error_type);

int main() {
    WSADATA wsa;
    SOCKET sockfd;
    struct sockaddr_in server_addr;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed to initialize Winsock. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    struct timeval timeout;
    timeout.tv_sec = ACK_TIMEOUT / 1000;
    timeout.tv_usec = (ACK_TIMEOUT % 1000) * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    send_data_packets(sockfd, &server_addr);

    closesocket(sockfd);
    WSACleanup();
    return 0;
}

void send_data_packets(SOCKET sockfd, struct sockaddr_in *server_addr) {
    DataPacket packet;
    int retries;
    int error_type = 0;

    for (unsigned short segment_no = 1; segment_no <= 5; segment_no++) {
        retries = 0;
        int ack_received = 0;

        while (retries < MAX_RETRIES && !ack_received) {
            packet.start_id = START_PACKET_ID;
            packet.client_id = 1;
            packet.type = DATA_PACKET;
            packet.segment_no = segment_no;
            snprintf(packet.payload, sizeof(packet.payload), "Data for packet %d", segment_no);
            packet.length = strlen(packet.payload);
            packet.end_id = END_PACKET_ID;

            if (segment_no > 1) {
                simulate_errors(&packet, error_type);
                error_type = (error_type + 1) % 4;
            }

            if (sendto(sockfd, (char *)&packet, sizeof(packet), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) == SOCKET_ERROR) {
                printf("Failed to send packet. Error Code: %d\n", WSAGetLastError());
                return;
            }

            printf("Packet %d sent. Waiting for ACK...\n", segment_no);
            ack_received = receive_response(sockfd, segment_no);

            if (!ack_received) {
                retries++;
                printf("Retrying packet %d (%d/%d)\n", segment_no, retries, MAX_RETRIES);
            }
        }

        if (!ack_received) {
            printf("Server does not respond. Aborting.\n");
            return;
        }
    }
}

int receive_response(SOCKET sockfd, unsigned short expected_segment_no) {
    char buffer[MAX_PACKET_SIZE];
    struct sockaddr_in server_addr;
    int addr_len = sizeof(server_addr);
    
    int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &addr_len);
    if (bytes_received == SOCKET_ERROR) {
        printf("ACK not received for packet %d (Timeout)\n", expected_segment_no);
        return 0;
    }

    unsigned short response_type = *(unsigned short *)(buffer + 4);
    unsigned short response_segment = *(unsigned short *)(buffer + 6);

    if (response_type == ACK_PACKET && response_segment == expected_segment_no) {
        printf("ACK received for packet %d\n", expected_segment_no);
        return 1;
    } else if (response_type == REJECT_PACKET) {
        unsigned short reject_sub_code = *(unsigned short *)(buffer + 8);
        printf("Reject received for packet %d, sub-code: 0x%X\n", response_segment, reject_sub_code);
    }

    return 0;
}

void simulate_errors(DataPacket *packet, int error_type) {
    switch (error_type) {
        case 0:
            packet->segment_no += 1;
            printf("Simulating out-of-sequence error for packet %d\n", packet->segment_no);
            break;
        case 1:
            packet->length += 5;
            printf("Simulating length mismatch error for packet %d\n", packet->segment_no);
            break;
        case 2:
            packet->end_id = 0xFFF0;
            printf("Simulating missing end identifier error for packet %d\n", packet->segment_no);
            break;
        case 3:
            packet->segment_no -= 1;
            printf("Simulating duplicate packet error for packet %d\n", packet->segment_no);
            break;
    }
}
