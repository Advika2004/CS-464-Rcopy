
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_PACKET_SIZE 1407  // As per spec

typedef struct {
    int sequence_number;
    int data_size;
    int ACK;
    uint8_t data[MAX_PACKET_SIZE];
} Packet;

typedef struct ReceiverBuffer {
    Packet **buffer;
    int buffer_size;
    int window_size;
    int expected;
    int highest;
} ReceiverBuffer;

typedef struct SenderWindow {
    Packet *buffer;
    int window_size;
    int lower;
    int upper;
    int current;
} SenderWindow;

// Receiver functions
ReceiverBuffer* initReceiverBuffer(int buffer_size, int window_size);
void storeOutOfOrderPacket(ReceiverBuffer *buffer, Packet *packet);
int retrieveOrderedPacket(ReceiverBuffer *buffer, FILE *outputFile);
void destroyReceiverBuffer(ReceiverBuffer *buffer);

// Sender functions
SenderWindow* initSenderWindow(int window_size);
void insertPacketIntoWindow(SenderWindow *window, int sequence_number, const uint8_t *data, int data_size);
int checkPacketACKStatus(SenderWindow *window, int sequence_number);
void acknowledgePacket(SenderWindow *window, int sequence_number);
void advanceSenderWindow(SenderWindow *window, int new_lower);
int canSendMorePackets(SenderWindow *window);
void destroySenderWindow(SenderWindow *window);
void printReceiverBuffer(ReceiverBuffer *buffer);

