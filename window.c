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
    buffer->buffer = (Packet**) calloc(window_size, sizeof(Packet*));
    if (!buffer->buffer) {
        perror("Failed to allocate memory for buffer");
        free(buffer);
        return NULL;
    }

    // go through and make sure that each pointer points to an empty packet object with the max size allowed?
    int i;
    for (i = 0; i < window_size; i++) {
        buffer->buffer[i] = (Packet*) malloc(sizeof(Packet));
        if (!buffer->buffer[i]) {
            perror("Failed to allocate memory for packet storage");
            int j;
            for (j = 0; j < i; j++) free(buffer->buffer[j]); // Free previous allocations
            free(buffer->buffer);
            free(buffer);
            return NULL;
        }

        // Initialize packet fields
        buffer->buffer[i]->sequence_number = -1;
        buffer->buffer[i]->data_size       = 0;
        buffer->buffer[i]->ACK            = 0;
        buffer->buffer[i]->valid          = 0; // Mark invalid initially
        memset(buffer->buffer[i]->data, 0, MAX_PACKET_SIZE);
    }

    buffer->buffer_size = buffer_size;
    buffer->window_size = window_size; 
    buffer->expected = 0;
    buffer->highest = -1;
    return buffer;
}

//takes in a packet and puts it in the buffer
void insertReceiverPacket(ReceiverBuffer *buffer, Packet *packet) {
    if (!buffer || !packet) return;

    int seq = packet->sequence_number;
    // If packet is *way* beyond the current expected+window, discard
    if (seq > buffer->expected + buffer->window_size) {
        printf("Packet %d beyond receiver window (expected up to %d). Discarding.\n",
               seq, buffer->expected + buffer->window_size);
        return;
    }

    int index = seq % buffer->window_size;

    // Fill the packet slot carefully; don't just blindly memcpy if you want to preserve your own fields
    buffer->buffer[index]->sequence_number = packet->sequence_number;
    buffer->buffer[index]->data_size       = packet->data_size;
    buffer->buffer[index]->ACK            = packet->ACK;
    buffer->buffer[index]->valid          = 1;  // Mark this slot as valid
    memcpy(buffer->buffer[index]->data, packet->data, packet->data_size);

    // Update highest if needed
    if (seq > buffer->highest) {
        buffer->highest = seq;
    }
}


//gets the next packet and writes it to the file 
// makes sure that when things get written to the file they are written in order
int flushBuffer(ReceiverBuffer *buffer, FILE *outputFile, int data_size) {
    if (!buffer || !outputFile) {
        return 0; // Error check for null pointers
    }
    
    printf("Flushing buffer starting from sequence number: %d\n", buffer->expected + 1);
    
    // Start flushing from the next expected packet (the one after what was just written)
    buffer->expected++;
    
    int packets_flushed = 0;
    int index;
    int found_packet = 1;
    
    // Continue flushing sequential packets until we find a gap
    while (found_packet) {
        index = buffer->expected % buffer->window_size;
        found_packet = 0;
        
        // Check if this slot has the expected packet
        if (buffer->buffer[index] && buffer->buffer[index]->sequence_number == buffer->expected && buffer->buffer[index]->valid == 1) {
            found_packet = 1;
            
            // Write the packet data to file (skipping the 7-byte header)
            printf("Flushing packet %d to file.\n", buffer->expected);
            fwrite(buffer->buffer[index]->data + 7, 1, 
                   buffer->buffer[index]->data_size - 7, outputFile);
            
            // Clear the packet data
            memset(buffer->buffer[index]->data, 0, MAX_PACKET_SIZE);
            buffer->buffer[index]->sequence_number = -1; // Mark as empty
            // mark as invalid after flushing 
            buffer->buffer[index]->valid = 0;
            buffer->buffer[index]->data_size = 0;
            
            // Move to the next packet
            buffer->expected++;
            packets_flushed++;
        } else {
            printf("Stopping flush: Missing packet at expected %d\n", buffer->expected);
            break;
        }
    }
    
    printf("Flushed %d packets to file\n", packets_flushed);
    return packets_flushed;
}



// Free the receiver buffer
void freeReceiverBuffer(ReceiverBuffer *buffer) {
    int i;
    for (i = 0; i < buffer->window_size; i++) {
        if (buffer->buffer[i]) {
            free(buffer->buffer[i]);
        }
    }
    free(buffer->buffer);
    free(buffer);
}

//makes the sender buffer
//! so is the window size only relevant to the sender and the receiver's buffer just grows depending on what comes in out of order? 
// Initialize the sender buffer (same as receiver buffer logic)
SenderWindow* initSenderWindow(int window_size, int buffer_size) {
    // Allocate memory for the sender structure
    SenderWindow *window = (SenderWindow*) malloc(sizeof(SenderWindow));
    if (!window) {
        perror("Failed to allocate sender window");
        return NULL;
    }

    // Allocate an array of pointers to packets (not actual packets)
    window->buffer = (Packet**) calloc(window_size, sizeof(Packet*));
    if (!window->buffer) {
        perror("Failed to allocate sender buffer");
        free(window);
        return NULL;
    }

    // Allocate each individual packet in the window
    int i;
    for (i = 0; i < window_size; i++) {
        window->buffer[i] = (Packet*) malloc(sizeof(Packet) + buffer_size + 7);
        if (!window->buffer[i]) {
            perror("Failed to allocate memory for packet storage");
            int j;
            for (j = 0; j < i; j++) free(window->buffer[j]); // Free previous allocations
            free(window->buffer);
            free(window);
            return NULL;
        }
    }

    window->window_size = window_size;
    window->lower = 0;
    window->upper = window_size;
    window->current = 0;

    return window;
}

//put a packet in the window and fill in the struct details
void insertPacketIntoWindow(SenderWindow *window, int sequence_number, const uint8_t *data, int data_size) {
    int index = sequence_number % window->window_size;

    if (!window->buffer[index]) {
        printf("buffer not allocated properly\n");
    }

    // fill in the packet
    // all packets start out not being acknowledged 
    window->buffer[index]->sequence_number = sequence_number;
    memcpy(window->buffer[index]->data, data, data_size);
    window->buffer[index]->data_size = data_size;
    window->buffer[index]->ACK = 0;  
}

// go into window and change the ack flag (happens after you get RR)
void acknowledgePacket(SenderWindow *window, int sequence_number) {
    int index = sequence_number % window->window_size;

    // Ensure packet exists before marking as acknowledged
    if (window->buffer[index]) {
        window->buffer[index]->ACK = 1;
    }
    printf("nothing exists at that index\n");
}


// check if a packet has been acknowledged
int checkPacketACKStatus(SenderWindow *window, int sequence_number) {
    int index = sequence_number % window->window_size;

    // Ensure packet exists before checking its ACK status
    if (window->buffer[index]) {
        return window->buffer[index]->ACK;
    }
    return 0;  // Default to not acknowledged if packet isn't stored
}

// slide the window forward based on RR
// would pass in the sequence number of the most recent RR
void advanceSenderWindow(SenderWindow *window, int new_lower) {
    if (new_lower > window->lower) {
        // Free up old packets as the window slides forward
        int i;
        for (i = window->lower; i < new_lower; i++) {
            int index = i % window->window_size;
            if (window->buffer[index]) {
                free(window->buffer[index]);
                window->buffer[index] = NULL;
            }
        }
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
    int i;
    for (i = 0; i < window->window_size; i++) {
        if (window->buffer[i]) {
            free(window->buffer[i]);
        }
    }
    free(window->buffer);
    free(window);
}


void printReceiverBuffer(ReceiverBuffer *buffer) {
    if (!buffer) {
        printf("Receiver buffer is NULL.\n");
        return;
    }

    printf("\n--- Receiver Buffer State ---\n");
    printf("Expected: %d, Highest: %d\n", buffer->expected, buffer->highest);
    int i;

    for (i = 0; i < buffer->window_size; i++) {
        Packet *pkt = buffer->buffer[i];

        if (pkt && pkt->valid) {
            printf("Slot %d -> Seq: %d | Size: %d | Valid: %d\n",
                   i, pkt->sequence_number, pkt->data_size, pkt->valid);
        } else {
            printf("Slot %d -> EMPTY\n", i);
        }
    }
    printf("----------------------------\n\n");
}


void printSenderBuffer(SenderWindow *window) {
    printf("\n--- Sender Window State ---\n");
    int i;
    for (i = 0; i < window->window_size; i++) {
        if (window->buffer[i]) { // Check if a packet exists in this slot
            printf("Slot %d -> Seq: %d | Size: %d | ACK: %d\n",
                   i, window->buffer[i]->sequence_number, window->buffer[i]->data_size, window->buffer[i]->ACK);
        } else {
            printf("Slot %d -> EMPTY\n", i);
        }
    }
    printf("Window Range: Lower=%d, Upper=%d, Current=%d\n", window->lower, window->upper, window->current);
    printf("----------------------------\n\n");
}
