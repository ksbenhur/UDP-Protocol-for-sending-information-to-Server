/* Updated UDP Server Implementation - Assignment 1 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib") // Link with Winsock library

#define SERVER_PORT 8080
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

// Packet structure for receiving data
typedef struct {
    unsigned short start_id;
    unsigned char client_id;
    unsigned short type;
    unsigned short segment_no;
    unsigned char length;
    char payload[MAX_PAYLOAD_SIZE];
    unsigned short end_id;
} DataPacket;

void process_packets(SOCKET sockfd);
void send_ack(SOCKET sockfd, struct sockaddr_in *client_addr, unsigned short segment_no);
void send_reject(SOCKET sockfd, struct sockaddr_in *client_addr, unsigned short segment_no, unsigned short reject_sub_code);

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

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed. Error Code: %d\n", WSAGetLastError());
        closesocket(sockfd);
        return 1;
    }

    printf("Server is running on port %d...\n", SERVER_PORT);
    process_packets(sockfd);

    closesocket(sockfd);
    WSACleanup();
    return 0;
}

void process_packets(SOCKET sockfd) {
    DataPacket packet;
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    unsigned short expected_segment_no = 1;

    while (1) {
        if (recvfrom(sockfd, (char *)&packet, sizeof(packet), 0, (struct sockaddr *)&client_addr, &addr_len) == SOCKET_ERROR) {
            printf("Failed to receive packet. Error Code: %d\n", WSAGetLastError());
            continue;
        }

        // Validate packet
        if (packet.start_id != START_PACKET_ID) {
            printf("Invalid start of packet identifier\n");
            continue;
        }

        if (packet.end_id != END_PACKET_ID) {
            printf("Missing end of packet identifier for segment %d\n", packet.segment_no);
            send_reject(sockfd, &client_addr, packet.segment_no, REJECT_END_MISSING);
            continue;
        }

        if (packet.segment_no < expected_segment_no) {
            printf("Duplicate packet received: segment %d\n", packet.segment_no);
            send_reject(sockfd, &client_addr, packet.segment_no, REJECT_DUPLICATE_PACKET);
            continue;
        }

        if (packet.segment_no > expected_segment_no) {
            printf("Out-of-sequence packet received: segment %d\n", packet.segment_no);
            send_reject(sockfd, &client_addr, packet.segment_no, REJECT_OUT_OF_SEQUENCE);
            continue;
        }

        if (packet.length != strlen(packet.payload)) {
            printf("Length mismatch for segment %d\n", packet.segment_no);
            send_reject(sockfd, &client_addr, packet.segment_no, REJECT_LENGTH_MISMATCH);
            continue;
        }

        // Valid packet received
        printf("Valid packet received: segment %d\n", packet.segment_no);
        send_ack(sockfd, &client_addr, packet.segment_no);
        
        expected_segment_no++; // Move to the next expected segment
    }
}

void send_ack(SOCKET sockfd, struct sockaddr_in *client_addr, unsigned short segment_no) {
    unsigned short ack_packet[] = {
        START_PACKET_ID, 1, ACK_PACKET, segment_no, END_PACKET_ID
    };

    if (sendto(sockfd, (char *)ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)client_addr, sizeof(*client_addr)) == SOCKET_ERROR) {
        printf("Failed to send ACK. Error Code: %d\n", WSAGetLastError());
    } else {
        printf("ACK sent for segment %d\n", segment_no);
    }
}

void send_reject(SOCKET sockfd, struct sockaddr_in *client_addr, unsigned short segment_no, unsigned short reject_sub_code) {
    unsigned short reject_packet[] = {
        START_PACKET_ID, 1, REJECT_PACKET, segment_no, reject_sub_code, END_PACKET_ID
    };

    if (sendto(sockfd, (char *)reject_packet, sizeof(reject_packet), 0, (struct sockaddr *)client_addr, sizeof(*client_addr)) == SOCKET_ERROR) {
        printf("Failed to send REJECT. Error Code: %d\n", WSAGetLastError());
    } else {
        printf("REJECT sent for segment %d with sub-code 0x%X\n", segment_no, reject_sub_code);
    }
}
