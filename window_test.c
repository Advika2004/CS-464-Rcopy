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

    test_init();
    test_ordered_packets();
    test_out_of_order_packets();
    test_discard_out_of_window();
    test_duplicate_packets();
    test_sender_window_ack();
    test_srej_handling();
    test_window_loss_and_retransmit();

    printf("\n--- All Tests Completed ---\n");

    return 0;
}

// Test 1: Initialize Sender and Receiver Buffers
void test_init() {
    printf("\n[Test] Initializing Sender and Receiver Buffers...\n");

    SenderWindow *sender = initSenderWindow(TEST_WINDOW_SIZE);
    ReceiverBuffer *receiver = initReceiverBuffer(TEST_BUFFER_SIZE, TEST_WINDOW_SIZE);

    if (sender && receiver) {
        printf("✅ Success: Buffers initialized properly.\n");
    } else {
        printf("❌ Failed: Buffers did not initialize correctly.\n");
    }

    destroySenderWindow(sender);
    destroyReceiverBuffer(receiver);
}

// Test 2: Ordered Packet Reception
void test_ordered_packets() {
    printf("\n[Test] Ordered Packet Reception...\n");

    ReceiverBuffer *receiver = initReceiverBuffer(TEST_WINDOW_SIZE, TEST_BUFFER_SIZE);
    FILE *output = fopen("test_output.txt", "w");

    uint8_t data[TEST_BUFFER_SIZE] = {0};
    int i;

    for (i = 0; i < TEST_WINDOW_SIZE; i++) {
        storeOutOfOrderPacket(receiver, i, data, TEST_BUFFER_SIZE);
    }
    int x;
    for (x = 0; i < TEST_WINDOW_SIZE; x++) {
        if (retrieveOrderedPacket(receiver, output)) {
            printf("Success: Retrieved packet %d in order.\n", x);
        } else {
            printf("Failed: Could not retrieve packet %d.\n", x);
        }
    }

    fclose(output);
    destroyReceiverBuffer(receiver);
}

// Test 3: Out-of-Order Packet Handling
void test_out_of_order_packets() {
    printf("\n[Test] Out-of-Order Packet Handling...\n");

    ReceiverBuffer *receiver = initReceiverBuffer(TEST_WINDOW_SIZE, TEST_BUFFER_SIZE);
    FILE *output = fopen("test_output.txt", "w");

    uint8_t data[TEST_BUFFER_SIZE] = {0};

    storeOutOfOrderPacket(receiver, 0, data, TEST_BUFFER_SIZE);
    storeOutOfOrderPacket(receiver, 2, data, TEST_BUFFER_SIZE);
    storeOutOfOrderPacket(receiver, 1, data, TEST_BUFFER_SIZE);

    if (retrieveOrderedPacket(receiver, output)) {
        printf("✅ Success: Retrieved packet 0 first.\n");
    }
    if (retrieveOrderedPacket(receiver, output)) {
        printf("✅ Success: Retrieved packet 1 after packet 0.\n");
    }
    if (retrieveOrderedPacket(receiver, output)) {
        printf("✅ Success: Retrieved packet 2 last.\n");
    }

    fclose(output);
    destroyReceiverBuffer(receiver);
}

// Test 4: Discarding Packets Beyond Window Size
void test_discard_out_of_window() {
    printf("\n[Test] Discarding Packets Beyond Window Size...\n");

    ReceiverBuffer *receiver = initReceiverBuffer(TEST_BUFFER_SIZE, TEST_WINDOW_SIZE);
    uint8_t data[TEST_BUFFER_SIZE] = {0};

    storeOutOfOrderPacket(receiver, 1, data, TEST_BUFFER_SIZE);
    storeOutOfOrderPacket(receiver, 2, data, TEST_BUFFER_SIZE);
    storeOutOfOrderPacket(receiver, 8, data, TEST_BUFFER_SIZE); // Out of window, should be discarded

    printf("receiver highes: %d\n", receiver->highest);
    if (receiver->highest == 2) {
        printf("✅ Success: Packet beyond window was discarded.\n");
    } else {
        printf("❌ Failed: Incorrectly stored packet outside window.\n");
    }

    destroyReceiverBuffer(receiver);
}

// Test 5: Handling Duplicate Packets
void test_duplicate_packets() {
    printf("\n[Test] Handling Duplicate Packets...\n");

    ReceiverBuffer *receiver = initReceiverBuffer(TEST_WINDOW_SIZE, TEST_BUFFER_SIZE);
    uint8_t data[TEST_BUFFER_SIZE] = {0};

    storeOutOfOrderPacket(receiver, 2, data, TEST_BUFFER_SIZE);
    storeOutOfOrderPacket(receiver, 2, data, TEST_BUFFER_SIZE); // Duplicate packet

    if (receiver->buffer[2 % TEST_WINDOW_SIZE]->sequence_number == 2) {
        printf("✅ Success: Duplicate packet ignored correctly.\n");
    } else {
        printf("❌ Failed: Duplicate packet overwritten.\n");
    }

    destroyReceiverBuffer(receiver);
}

// Test 6: Advancing the Sender Window on ACK
void test_sender_window_ack() {
    printf("\n[Test] Advancing Sender Window on ACK...\n");

    SenderWindow *sender = initSenderWindow(TEST_WINDOW_SIZE);
    uint8_t data[TEST_BUFFER_SIZE] = {0};

    insertPacketIntoWindow(sender, 0, data, TEST_BUFFER_SIZE);
    insertPacketIntoWindow(sender, 1, data, TEST_BUFFER_SIZE);
    insertPacketIntoWindow(sender, 2, data, TEST_BUFFER_SIZE);

    acknowledgePacket(sender, 0);
    acknowledgePacket(sender, 1);
    advanceSenderWindow(sender, 2);

    if (sender->lower == 2) {
        printf("✅ Success: Sender window advanced to new lower edge.\n");
    } else {
        printf("❌ Failed: Sender window did not advance correctly.\n");
    }

    destroySenderWindow(sender);
}

// Test 7: Handling Selective Reject (SREJ)
void test_srej_handling() {
    printf("\n[Test] Handling Selective Reject (SREJ)...\n");

    ReceiverBuffer *receiver = initReceiverBuffer(TEST_WINDOW_SIZE, TEST_BUFFER_SIZE);
    uint8_t data[TEST_BUFFER_SIZE] = {0};

    storeOutOfOrderPacket(receiver, 1, data, TEST_BUFFER_SIZE);
    storeOutOfOrderPacket(receiver, 3, data, TEST_BUFFER_SIZE);
    storeOutOfOrderPacket(receiver, 4, data, TEST_BUFFER_SIZE);

    if (receiver->expected == 0) {
        printf("✅ Success: Receiver expects packet 0 before processing 1, 3, 4.\n");
    } else {
        printf("❌ Failed: Receiver processing out of order.\n");
    }

    destroyReceiverBuffer(receiver);
}

// Test 8: Simulating Window Loss and Retransmission
void test_window_loss_and_retransmit() {
    printf("\n[Test] Simulating Window Loss and Retransmission...\n");

    SenderWindow *sender = initSenderWindow(TEST_WINDOW_SIZE);
    ReceiverBuffer *receiver = initReceiverBuffer(TEST_WINDOW_SIZE, TEST_BUFFER_SIZE);
    uint8_t data[TEST_BUFFER_SIZE] = {0};

    insertPacketIntoWindow(sender, 0, data, TEST_BUFFER_SIZE);
    insertPacketIntoWindow(sender, 1, data, TEST_BUFFER_SIZE);
    insertPacketIntoWindow(sender, 2, data, TEST_BUFFER_SIZE);

    // Simulate loss of RR
    printf("Simulating lost RR...\n");

    // Simulate retransmission
    printf("Retransmitting packets...\n");
    int i;
    for (i = 0; i < TEST_WINDOW_SIZE; i++) {
        if (!checkPacketACKStatus(sender, i)) {
            printf("✅ Success: Retransmitting packet %d\n", i);
        }
    }

    destroySenderWindow(sender);
    destroyReceiverBuffer(receiver);
}
