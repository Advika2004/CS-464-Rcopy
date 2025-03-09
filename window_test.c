#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "window.h"

#define TEST_WINDOW_SIZE 5
#define TEST_BUFFER_SIZE 1000

void test_init();
void test_ordered_packets();
void test_out_of_order_packets();
void test_discard_out_of_window();
void test_duplicate_packets();
void test_sender_window_ack();
void test_srej_handling();
void test_window_loss_and_retransmit();

int main() {
    printf("\n--- Running Windowing Tests ---\n");

    //test_init();
    //test_ordered_packets();
    //test_out_of_order_packets();
    //test_discard_out_of_window();
    // test_duplicate_packets();
    // test_sender_window_ack();
    // test_srej_handling();
    // test_window_loss_and_retransmit();

    printf("\n--- All Tests Completed ---\n");

    return 0;
}

//Test 1: Initialize Sender and Receiver Buffers
// void test_init() {
//     printf("\n[Test] Initializing Sender and Receiver Buffers...\n");

//     SenderWindow *sender = initSenderWindow(TEST_WINDOW_SIZE, TEST_BUFFER_SIZE);
//     ReceiverBuffer *receiver = initReceiverBuffer(TEST_BUFFER_SIZE, TEST_WINDOW_SIZE);

//     if (sender && receiver) {
//         printf("Success: Buffers initialized properly.\n");
//     } else {
//         printf("Failed: Buffers did not initialize correctly.\n");
//     }

//     printReceiverBuffer(receiver);
//     printSenderBuffer(sender);


//     destroySenderWindow(sender);
//     freeReceiverBuffer(receiver);
// }

// Test 2: Handling Large Sequence Numbers
// Test 2: Ordered Packet Reception and Flushing Buffer
// void test_ordered_packets() {
//     printf("\n[Test] Ordered Packet Reception and Flushing Buffer...\n");

//     // Initialize receiver buffer
//     ReceiverBuffer *receiver = initReceiverBuffer(TEST_BUFFER_SIZE, TEST_WINDOW_SIZE);
//     FILE *output = fopen("test_output.txt", "w");

//     // Expected start is 4, so 5, 6, 7 will be stored but not flushed
//     receiver->expected = 4; // Manually setting expected to 4

//     // Insert packets 5, 6, and 7
//     printf("Inserting packets 5, 6, and 7 (waiting for 4)...\n");
//     int i;
//     for (i = 0; i < 3; i++) {
//         Packet packet;
//         packet.sequence_number = i + 5;  // 5, 6, 7
//         packet.data_size = TEST_BUFFER_SIZE;
//         packet.ACK = 1;
//         // Fill with test data
//         memset(packet.data, 'A' + i, TEST_BUFFER_SIZE);
        
//         // Insert the packet
//         insertReceiverPacket(receiver, &packet);
//     }

//     // Print buffer state after inserting 5, 6, 7
//     printf("\nBuffer state before receiving packet 4:\n");
//     printReceiverBuffer(receiver);

//     printf("\npretend we got 4 and then wrote 4 to disc now flushing buffer:\n");
//     flushBuffer(receiver, output, TEST_BUFFER_SIZE);

//     // Print final buffer state
//     printf("\nBuffer state after flushing:\n");
//     printReceiverBuffer(receiver);

//     fclose(output);
//     freeReceiverBuffer(receiver);
// }

// Test 3: Out-of-Order Packet Handling
// void test_out_of_order_packets() {
//     printf("\n[Test] Out-of-Order Packet Handling...\n");

//     ReceiverBuffer *receiver = initReceiverBuffer(TEST_BUFFER_SIZE, TEST_WINDOW_SIZE);
//     FILE *output = fopen("test_output.txt", "w");

//     uint8_t data[TEST_BUFFER_SIZE] = {0};

//     Packet packet1 = {.sequence_number = 1, .data_size = TEST_BUFFER_SIZE, .ACK = 1};
//     Packet packet2 = {.sequence_number = 2, .data_size = TEST_BUFFER_SIZE, .ACK = 1};
//     Packet packet3 = {.sequence_number = 3, .data_size = TEST_BUFFER_SIZE, .ACK = 1};

//     memcpy(packet1.data, data, TEST_BUFFER_SIZE);
//     memcpy(packet2.data, data, TEST_BUFFER_SIZE);
//     memcpy(packet3.data, data, TEST_BUFFER_SIZE);

//     printf("Inserting packets in order: 0, 2, 1\n");
//     insertReceiverPacket(receiver, &packet3);
//     insertReceiverPacket(receiver, &packet1);
//     insertReceiverPacket(receiver, &packet2);

//     int packets_flushed = flushBuffer(receiver, output, TEST_BUFFER_SIZE);
//     if (packets_flushed == 3) {
//         printf("Success: Retrieved and flushed 3 packets in correct order.\n");
//     } else {
//         printf("Failed: Flushed %d packets instead of expected 3.\n", packets_flushed);
//     }

//     fclose(output);
//     freeReceiverBuffer(receiver);
// }

// // Test 4: Discarding Packets Beyond Window Size
// void test_discard_out_of_window() {
//     printf("\n[Test] Discarding Packets Beyond Window Size...\n");

//     ReceiverBuffer *receiver = initReceiverBuffer(TEST_BUFFER_SIZE, TEST_WINDOW_SIZE);
//     uint8_t data[TEST_BUFFER_SIZE] = {0};

//     // Set expected to 0 (default)
//     receiver->expected = 0;

//     Packet packet1 = {.sequence_number = 1, .data_size = TEST_BUFFER_SIZE, .ACK = 1};
//     Packet packet2 = {.sequence_number = 2, .data_size = TEST_BUFFER_SIZE, .ACK = 1};
//     Packet packet8 = {.sequence_number = 8, .data_size = TEST_BUFFER_SIZE, .ACK = 1}; // Beyond window size

//     memcpy(packet1.data, data, TEST_BUFFER_SIZE);
//     memcpy(packet2.data, data, TEST_BUFFER_SIZE);
//     memcpy(packet8.data, data, TEST_BUFFER_SIZE);

//     printf("Inserting packets 1, 2, and 8 (with window size %d)\n", TEST_WINDOW_SIZE);
//     insertReceiverPacket(receiver, &packet1);
//     insertReceiverPacket(receiver, &packet2);
//     insertReceiverPacket(receiver, &packet8);

//     // Get index where packet 8 would be stored
//     int index8 = 8 % TEST_WINDOW_SIZE;
//     if (receiver->buffer[index8]->sequence_number != 8) {
//         printf("Success: Packet beyond window was discarded.\n");
//     } else {
//         printf("Failed: Incorrectly stored packet outside window (sequence %d).\n", 
//                 receiver->buffer[index8]->sequence_number);
//     }

//     freeReceiverBuffer(receiver);
// }

// // Test 5: Handling Duplicate Packets
// void test_duplicate_packets() {
//     printf("\n[Test] Handling Duplicate Packets...\n");

//     ReceiverBuffer *receiver = initReceiverBuffer(TEST_BUFFER_SIZE, TEST_WINDOW_SIZE);
//     uint8_t data[TEST_BUFFER_SIZE] = {0};

//     // Create first packet with sequence 2
//     Packet packet1 = {.sequence_number = 2, .data_size = TEST_BUFFER_SIZE, .ACK = 1};
//     memcpy(packet1.data, data, TEST_BUFFER_SIZE);
//     memset(packet1.data, 'A', 10); // Mark first 10 bytes with 'A'

//     // Create "duplicate" packet with different content
//     Packet packet2 = {.sequence_number = 2, .data_size = TEST_BUFFER_SIZE, .ACK = 1};
//     memcpy(packet2.data, data, TEST_BUFFER_SIZE);
//     memset(packet2.data, 'B', 10); // Mark first 10 bytes with 'B'

//     printf("Inserting original packet with sequence 2\n");
//     insertReceiverPacket(receiver, &packet1);
    
//     printf("Inserting duplicate packet with sequence 2\n");
//     insertReceiverPacket(receiver, &packet2);

//     // Check which version is in the buffer
//     int index = 2 % TEST_WINDOW_SIZE;
//     if (receiver->buffer[index]->data[0] == 'B') {
//         printf("Success: Duplicate packet overwrote original (normal sliding window behavior).\n");
//     } else if (receiver->buffer[index]->data[0] == 'A') {
//         printf("Note: Original packet was preserved (duplicate ignored).\n");
//     } else {
//         printf("Failed: Unexpected data in buffer.\n");
//     }

//     freeReceiverBuffer(receiver);
// }

// Test 6: Advancing the Sender Window on ACK
// void test_sender_window_ack() {
//     printf("\n[Test] Advancing Sender Window on ACK...\n");

//     SenderWindow *sender = initSenderWindow(TEST_WINDOW_SIZE, TEST_BUFFER_SIZE);
//     uint8_t data[TEST_BUFFER_SIZE] = {0};

//     insertPacketIntoWindow(sender, 0, data, TEST_BUFFER_SIZE);
//     insertPacketIntoWindow(sender, 1, data, TEST_BUFFER_SIZE);
//     insertPacketIntoWindow(sender, 2, data, TEST_BUFFER_SIZE);

//     acknowledgePacket(sender, 0);
//     acknowledgePacket(sender, 1);
//     advanceSenderWindow(sender, 2);

//     if (sender->lower == 2) {
//         printf("Success: Sender window advanced to new lower edge.\n");
//     } else {
//         printf("Failed: Sender window did not advance correctly.\n");
//     }

//     destroySenderWindow(sender);
// }

// // Test 7: SREJ Handling
// void test_srej_handling() {
//     printf("\n[Test] Handling Selective Reject (SREJ)...\n");

//     ReceiverBuffer *receiver = initReceiverBuffer(TEST_BUFFER_SIZE, TEST_WINDOW_SIZE);
//     uint8_t data[TEST_BUFFER_SIZE] = {0};

//     Packet packet1 = {.sequence_number = 1, .data_size = TEST_BUFFER_SIZE, .ACK = 1};
//     Packet packet3 = {.sequence_number = 3, .data_size = TEST_BUFFER_SIZE, .ACK = 1};
//     Packet packet4 = {.sequence_number = 4, .data_size = TEST_BUFFER_SIZE, .ACK = 1};

//     memcpy(packet1.data, data, TEST_BUFFER_SIZE);
//     memcpy(packet3.data, data, TEST_BUFFER_SIZE);
//     memcpy(packet4.data, data, TEST_BUFFER_SIZE);

//     printf("Inserting packets 1, 3, and 4 (with packet 0 missing)\n");
//     insertReceiverPacket(receiver, &packet1);
//     insertReceiverPacket(receiver, &packet3);
//     insertReceiverPacket(receiver, &packet4);

//     if (receiver->expected == 0) {
//         printf("Success: Receiver expects packet 0 before processing others.\n");
//     } else {
//         printf("Failed: Receiver unexpected value %d (should be 0).\n", receiver->expected);
//     }

//     freeReceiverBuffer(receiver);
// }

// // Test 8: Window Loss and Retransmission
// void test_window_loss_and_retransmit() {
//     printf("\n[Test] Simulating Window Loss and Retransmission...\n");

//     SenderWindow *sender = initSenderWindow(TEST_WINDOW_SIZE, TEST_BUFFER_SIZE);
//     uint8_t data[TEST_BUFFER_SIZE] = {0};

//     // Insert and mark some packets
//     printf("Inserting packets 0-4 into sender window\n");
//     int i;
//     for (i = 0; i < 5; i++) {
//         insertPacketIntoWindow(sender, i, data, TEST_BUFFER_SIZE);
//     }
    
//     // Acknowledge packets 0 and 1
//     acknowledgePacket(sender, 0);
//     acknowledgePacket(sender, 1);
    
//     printSenderBuffer(sender);

//     printf("Simulating retransmission of unacknowledged packets...\n");
//     int retransmitted = 0;
//     int j;
//     for (j = 0; j < 5; j++) {
//         if (!checkPacketACKStatus(sender, j)) {
//             printf("Retransmitting packet %d\n", j);
//             retransmitted++;
//         }
//     }
    
//     if (retransmitted == 3) {
//         printf("Success: 3 unacknowledged packets were retransmitted.\n");
//     } else {
//         printf("Failed: Expected 3 retransmissions, got %d.\n", retransmitted);
//     }

//     destroySenderWindow(sender);
// }