
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_PACKET_SIZE 1407  // As per spec

typedef struct {
    int sequence_number;
    int data_size;
    int valid; 
    int ACK;
    uint8_t data[MAX_PACKET_SIZE];
} Packet;

typedef struct {
    Packet **buffer;    // Array of pointers to Packet structs (dynamic allocation)
    int buffer_size;    // Total buffer size (same as window size)
    int window_size;    // Window size (defines how many packets can be in flight)
    int expected;       // Next expected packet in order (Receiver)
    int highest;        // Highest received sequence number (Receiver)
} ReceiverBuffer;

typedef struct {
    Packet **buffer;    // Array of pointers to Packet structs (dynamic allocation)
    int buffer_size;    // Total buffer size (same as window size)
    int window_size;    // Window size (defines how many packets can be in flight)
    int lower;          // Lowest unacknowledged sequence number (Sender)
    int upper;          // Upper edge (L + window size)
    int current;        // Next sequence number to send (Sender)
} SenderWindow;

// Receiver functions
ReceiverBuffer* initReceiverBuffer(int buffer_size, int window_size);
void insertReceiverPacket(ReceiverBuffer *buffer, Packet *packet);
int flushBuffer(ReceiverBuffer *buffer, FILE *outputFile, int buffer_size);
void freeReceiverBuffer(ReceiverBuffer *buffer);

// Sender functions
SenderWindow* initSenderWindow(int window_size, int buffer_size);
void insertPacketIntoWindow(SenderWindow *window, int sequence_number, const uint8_t *data, int data_size);
int checkPacketACKStatus(SenderWindow *window, int sequence_number);
void acknowledgePacket(SenderWindow *window, int sequence_number);
void advanceSenderWindow(SenderWindow *window, int new_lower);
int canSendMorePackets(SenderWindow *window);
void destroySenderWindow(SenderWindow *window);
void printReceiverBuffer(ReceiverBuffer *buffer);
void printSenderBuffer(SenderWindow *window);

