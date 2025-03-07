#include "window.h"

//will allocate memory for the buffer upon start
ReceiverBuffer* initReceiverBuffer(int buffer_size, int window_size) {

    //first make the struct to store the info
    ReceiverBuffer *buffer = (ReceiverBuffer*) malloc(sizeof(ReceiverBuffer));
    if (!buffer) {
        perror("Failed to allocate memory for receiver buffer");
        return NULL;
    }

    //this is the 2D part
    // make the buffer part of the struct an array of pointers 
    // initialize all the pointers to NULL with calloc
    buffer->buffer = (Packet**) calloc(buffer_size, sizeof(Packet*));
    if (!buffer->buffer) {
        perror("Failed to allocate memory for buffer");
        free(buffer);
        return NULL;
    }

    // go through and make sure that each pointer points to an empty packet object with the max size allowed?
    int i;
    for (i = 0; i < window_size; i++) {
        buffer->buffer[i] = (Packet*) malloc(sizeof(Packet) + (buffer_size + 7));
        if (!buffer->buffer[i]) {
            perror("Failed to allocate memory for packet storage");
            for (int j = 0; j < i; j++) free(buffer->buffer[j]); // Free previous allocations
            free(buffer->buffer);
            free(buffer);
            return NULL;
        }
    }

    buffer->buffer_size = buffer_size;
    buffer->window_size = window_size; 
    buffer->expected = 0;
    buffer->highest = -1;
    return buffer;
}

//takes in a packet and puts it in the buffer
void storeOutOfOrderPacket(ReceiverBuffer *buffer, Packet *packet) {
    // calculates what index to put it at
    int index = packet->sequence_number % buffer->buffer_size;
    printf("sequence number: %d\n", packet->sequence_number);
    printf("buffer expected: %d\n", buffer->expected);
    printf("buffer window size: %d\n", buffer->window_size);

    //check if the packet is past the window size
    if (packet->sequence_number > buffer->expected + buffer->window_size) {
        printf("Packet %d is beyond window range (expected: %d, window: %d). Discarding.\n",
               packet->sequence_number, buffer->expected, buffer->window_size);
        return;  // dont want to store that
    }

    // Copy the new packet into the preallocated buffer slot
    memcpy(buffer->buffer[index], packet, sizeof(Packet));


    // if the sequence number is higher than the past one, update it
    if (packet->sequence_number > buffer->highest) {
        buffer->highest = packet->sequence_number;
    }

    printReceiverBuffer(buffer);
}

//gets the next packet and writes it to the file 
// makes sure that when things get written to the file they are written in order
int retrieveOrderedPacket(ReceiverBuffer *buffer, FILE *outputFile) {
    int index = buffer->expected % buffer->buffer_size;

    if (buffer->buffer[index] && buffer->buffer[index]->ACK &&
        buffer->buffer[index]->sequence_number == buffer->expected) {

        fwrite(buffer->buffer[index]->data, 1, buffer->buffer[index]->data_size, outputFile);
        
        free(buffer->buffer[index]);  
        buffer->buffer[index] = NULL;
        buffer->expected++;

        return 1;
    }

    return 0;
}

// Free the receiver buffer
void destroyReceiverBuffer(ReceiverBuffer *buffer) {
    int i;
    for (i = 0; i < buffer->buffer_size; i++) {
        if (buffer->buffer[i]) {
            free(buffer->buffer[i]);
        }
    }
    free(buffer->buffer);
    free(buffer);
}

//makes the sender buffer
//! so is the window size only relevant to the sender and the receiver's buffer just grows depending on what comes in out of order? 
SenderWindow* initSenderWindow(int window_size) {
    SenderWindow *window = (SenderWindow*) malloc(sizeof(SenderWindow));
    if (!window) {
        perror("Failed to allocate sender window");
        return NULL;
    }

    window->buffer = (Packet*) calloc(window_size, sizeof(Packet));
    if (!window->buffer) {
        perror("Failed to allocate sender buffer");
        free(window);
        return NULL;
    }

    window->window_size = window_size;
    window->lower = 0;
    window->upper = window_size - 1;
    window->current = 0;
    return window;
}

//put a packet in the window and fill in the struct details
void insertPacketIntoWindow(SenderWindow *window, int sequence_number, const uint8_t *data, int data_size) {
    int index = sequence_number % window->window_size;
    window->buffer[index].sequence_number = sequence_number;
    memcpy(window->buffer[index].data, data, data_size);
    window->buffer[index].ACK = 0;  
}

//packets get stored until they are acknowledged, so be able to go in and say that it has been acked
void acknowledgePacket(SenderWindow *window, int sequence_number) {
    int index = sequence_number % window->window_size;
    window->buffer[index].ACK = 1;
}

// check if a packet has been acknowledged
int checkPacketACKStatus(SenderWindow *window, int sequence_number) {
    int index = sequence_number % window->window_size;
    return window->buffer[index].ACK;
}

// slide the window forward based on RR
void advanceSenderWindow(SenderWindow *window, int new_lower) {
    if (new_lower > window->lower) {
        window->lower = new_lower;
        window->upper = (new_lower + window->window_size - 1);
    }
}

// Check if there is space in the sender's window
int canSendMorePackets(SenderWindow *window) {
    return (window->current < window->upper);
}

// Free the sender window
void destroySenderWindow(SenderWindow *window) {
    free(window->buffer);
    free(window);
}

void printReceiverBuffer(ReceiverBuffer *buffer) {
    printf("\n--- Receiver Buffer State ---\n");
    int i;
    for (i = 0; i < buffer->buffer_size; i++) {
        if (buffer->buffer[i] && buffer->buffer[i]->ACK) {
            printf("Slot %d -> Seq: %d | Size: %d | ACK: %d\n",
                   i, buffer->buffer[i]->sequence_number, buffer->buffer[i]->data_size, buffer->buffer[i]->ACK);
        } else {
            printf("Slot %d -> EMPTY\n", i);
        }
    }
    printf("----------------------------\n\n");
}

