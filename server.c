/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include "server.h"
#include "cpe464.h"

typedef enum {
	INIT,
    GET_FILENAME,
    DONE,
    SEND_DATA,
    WAIT_ON_ACK,
	WAIT_ON_EOF_ACK
} state;

int main ( int argc, char *argv[]  )
{ 
	int main_server_socket = 0;				

	ServerParams serverParam = checkArgs(argc, argv);
		
	main_server_socket = udpServerSetup(serverParam.port_number);

	// need to poll on the incoming socket for the filename connection
	setupPollSet();
	addToPollSet(main_server_socket);


	//need to call this here bcasue need to call this before calling sendErr?
	sendErr_init(serverParam.error_rate, DROP_ON, FLIP_OFF, DEBUG_OFF, RSEED_OFF);

	uint8_t buffer[114];	  
	struct sockaddr_in6 client;		
	socklen_t clientAddrLen = sizeof(struct sockaddr_in6);	

	// will extract the values from the client and store it in here
	char filename[MAX_FILENAME_LEN];
	uint16_t buffer_size = 0;
	uint32_t window_size = 0;

	signal(SIGCHLD, handleZombies);

	while(1){
	// need to poll forever ? so that i can manage all incoming clients?
	int poll = pollCall(-1);
	if (poll < 0) {
    	perror("pollCall failed");
    	exit(1);
	}


	//take in the filename packet from the client

    int recv_len = recvfrom(main_server_socket, buffer, 114, 0, (struct sockaddr*)&client, &clientAddrLen);
    if (recv_len < 0) {
        perror("recvfrom failed");
		continue;
    }

	printf("Received in MAIN SERVER PDU Buffer (Hex): ");
	int i;
    for (i = 0; i < recv_len; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n");

	//calculate the checksum and see if its valid 
	uint16_t result = calculateFilenameChecksumFILENAME(buffer);
	printf("result from checksum: %d\n", result);
	if (result != 0){
		printf("bad checksum in parent\n");
		continue;
	}
	else{
		// can proceed to extract the correct info
		memcpy(filename, buffer + 7, MAX_FILENAME_LEN);
		memcpy(&buffer_size, buffer + 107, 2);
		uint16_t host_buffer_size = ntohs(buffer_size);
		memcpy(&window_size, buffer + 109, 4);
		uint16_t host_window_size = ntohl(window_size);

		//!DEBUG: add print statements to check that it was read out properly
		printf("checksum calculated right: %d\n", result);

		printf("Extracted Filename: %s\n", filename);
		printf("Extracted Buffer Size: %u bytes\n", host_buffer_size);
		printf("Extracted Window Size: %u packets\n", host_window_size);

		startFSM(filename, host_buffer_size, host_window_size, &client, clientAddrLen, main_server_socket, serverParam);
	}
	}

	//! do I need this here? 
	close(main_server_socket);
	return 0;
}

uint16_t calculateFilenameChecksumFILENAME(uint8_t* buffer){
	uint16_t static temp_buff[MAX_HEADER_LEN + MAX_FILENAME_LEN + MAX_BUFFER_LEN + MAX_WINDOW_LEN + 1];
	memcpy(temp_buff, buffer, 113);
	uint8_t even_length = 0;
	memcpy(temp_buff + 113, &even_length, 1);
	uint16_t calculatedChecksum = in_cksum(temp_buff, sizeof(temp_buff));
	return calculatedChecksum;
}


void startFSM(char* filename, uint16_t buffer_size, uint32_t window_size, struct sockaddr_in6 * client, socklen_t clientAddrLen, int main_server_socket, ServerParams serverParam){
	int state;

	printf("[SERVER] Received filename request: %s\n", filename);
    printf("[SERVER] Buffer size: %d, Window size: %d\n", buffer_size, window_size);


	// need to make a child now
	pid_t server_child = fork();
	if (server_child < 0) {
		perror("fork failed");
		//! what to do when the fork fails? 
		return;
	}

	// now within the child start the fsm
	// close the main server socket since that is not used anymore in here
	if (server_child == 0) { 

		//Reinitialize sendtoErr in the child process**
    	sendtoErr_init(serverParam.error_rate, DROP_ON, FLIP_OFF, DEBUG_OFF, RSEED_ON);

		printf("[CHILD] New child process started with PID: %d\n", getpid());

		close(main_server_socket); 
		removeFromPollSet(main_server_socket);
			
		// Open a new socket for communication with this client
		//os will give it a random one
		child_server_socket = udpServerSetup(0);
		addToPollSet(child_server_socket);
		printf("[CHILD] Added child server socket (%d) to poll set.\n", child_server_socket);


		//send the "talk to here now" packet to the client
		uint8_t* temp_buff = makeTalkHereNowBeforeChecksum();
		uint16_t calculated_checksum = calculateFilenameChecksumACK(temp_buff);
		uint8_t* talk_buff_send = makeTalkHereNowAfterChecksum(temp_buff, calculated_checksum);

		printPDU(talk_buff_send);

		uint16_t result = calculateFilenameChecksumACK(talk_buff_send);
		if (result == 0){
			printf("checksum calculated right: %d\n", result);
		}
		else{
			printf("checksum not right\n");
			exit(1);
		}

		// Send the "talk to here" packet
		printf("sending the talk here now to client: %d\n", client->sin6_port);
		//safeSendto(socketNum, buffer, strlen(buffer)+1, 0, (struct sockaddr *) & client, clientAddrLen);
		int bytes_sent = sendtoErr(child_server_socket, talk_buff_send, 7, 0,
                           (struct sockaddr*) client, clientAddrLen);
		printf("Sent PDU Buffer (Hex): ");
		int i;
		for (i = 0; i < 7; i++) { // Print first 20 bytes
    	printf("%02X ", talk_buff_send[i]);
		}
		printf("\n");
		printf("bytes sent: %d\n", bytes_sent);
		if (bytes_sent < 0) {
    		perror("sendtoErr failed\n");
    		state = DONE;
		}

		state = GET_FILENAME;

	//while the state is not the done state
    while (state != DONE) {
		//check the state
        switch (state) {
            case GET_FILENAME:
				printf("made it to the opening file state\n");
				printf("ok now in here I can try and open the file and send the ack packet\n");
                state = doGetFilenameState(filename, client, clientAddrLen);
                break;
            case SEND_DATA:
                printf("send data\n");
                state = doSendDataState(filename, buffer_size, window_size, child_server_socket, client, clientAddrLen);
                break;
            case WAIT_ON_ACK:
                printf("waiting on ack\n");
				//state = doWaitOnAckState();
                break;
			case WAIT_ON_EOF_ACK:
                printf("waiting on eof ack\n");
				//state = doWaitOnEOFAckState();
                break;
        }
    }
	//still within child but not in FSM ? idk what to put here
	}
	// if not within the child, then keep polling
    return;
}


int doGetFilenameState(char* filename, struct sockaddr_in6 * client, socklen_t clientAddrLen) {
	printf("filename being passed to the child server: %s\n", filename);

	// try and open the file given, if possible then send back the yes ack packet and move to sending data
	// if not, send back the no ack packet and move to DONE
	FILE* curr_file_open = fopen(filename, "rb");

	if (curr_file_open == NULL){
		printf("Couldn't open file...sending error packet\n");

		uint8_t* temp_err_buff = makeERRORFilenameACKBeforeChecksum();
		uint16_t calculated_checksum = calculateFilenameChecksumACK(temp_err_buff);
		uint8_t *err_buff_send = makeFilenameACKAfterChecksum(temp_err_buff, calculated_checksum);

		printPDU(err_buff_send);

		uint16_t result = calculateFilenameChecksumACK(err_buff_send);
		if (result == 0){
			printf("checksum calculated right: %d\n", result);
		}
		else{
			printf("checksum not right\n");
			exit(1);
		}

		// Send the error ack packet
		int bytes_sent = sendtoErr(child_server_socket, err_buff_send, 7, 0,
                        (struct sockaddr*) client, clientAddrLen);
		printf("Sent PDU Buffer (Hex): ");
		int i;
		for (i = 0; i < 7; i++) { // Print first 20 bytes
   		printf("%02X ", err_buff_send[i]);
		}
		printf("\n");
		printf("bytes sent: %d\n", bytes_sent);
		if (bytes_sent < 0) {
    		perror("sendtoErr failed\n");
    		return DONE;
		}

		//! now poll for a response from the client about the file not ok

		int retries = 0;
    	while (retries < 10) {
			printf("child_server_socket: %d\n", child_server_socket);
        	int poll_result = pollCall(1000);
			printf("poll_result: %d\n", poll_result);
        	if (poll_result != -1) {
            	// Client responded, child can exit
            	uint8_t recv_buffer[ACK_BUFF_SIZE];
            	int received = recvfrom(child_server_socket, recv_buffer, ACK_BUFF_SIZE, 0, 
                                    (struct sockaddr*) client, &clientAddrLen);
            	if (received > 0) {
                	printf("[CHILD] Client acknowledged error packet. Terminating.\n");
                	return DONE;
            	}
        	}
        retries++;
    	}

    	printf("[CHILD] No response after 10 tries. Terminating.\n");
    	return DONE;
	}

	//else send the correct ack packet and move to waiting for data state
	// make the packet for the correct ack
	uint8_t *temp_buff = makeVALIDFilenameACKBeforeChecksum();
	uint16_t calculated_checksum = calculateFilenameChecksumACK(temp_buff);
	uint8_t *ack_buff_send = makeFilenameACKAfterChecksum(temp_buff, calculated_checksum);

	printPDU(ack_buff_send);

	uint16_t result = calculateFilenameChecksumACK(ack_buff_send);
	if (result == 0){
		printf("checksum calculated right: %d\n", result);
	}
	else{
		printf("checksum not right\n");
		exit(1);
	}

	// Send the correct ack packet
	//safeSendto(socketNum, buffer, strlen(buffer)+1, 0, (struct sockaddr *) & client, clientAddrLen);
	int bytes_sent = sendtoErr(child_server_socket, ack_buff_send, 7, 0,
                        (struct sockaddr*) client, clientAddrLen);
	printf("Sent PDU Buffer (Hex): ");
	int i;
	for (i = 0; i < 7; i++) { // Print first 20 bytes
    printf("%02X ", ack_buff_send[i]);
	}
	printf("\n");

	//! debug printing within here
	char ip_str[INET6_ADDRSTRLEN]; // Buffer to store the IP string

	// Convert IPv6 address to string
	inet_ntop(AF_INET6, &client->sin6_addr, ip_str, sizeof(ip_str));

	printf("Packet sent to Client IP: %s, Port: %d\n", ip_str, ntohs(client->sin6_port));
	//!debug printing above
	
	printf("bytes sent: %d\n", bytes_sent);
	if (bytes_sent < 0) {
    	perror("sendtoErr failed\n");
    	return DONE;
	}

	return SEND_DATA;
}

int doDoneState(int child_server_socket) {
	printf("[CHILD] Terminating child process with PID: %d\n", getpid());

    //Remove the child socket from the poll set
    removeFromPollSet(child_server_socket);
    printf("[CHILD] Removed child socket %d from poll set.\n", child_server_socket);

    //Close the child socket
    close(child_server_socket);
    printf("[CHILD] Closed child socket %d.\n", child_server_socket);

    //Exit the child process cleanly
    exit(0);
}








int doSendDataState(char *filename, uint16_t buffer_size, uint32_t window_size, int child_server_socket, struct sockaddr_in6 *client, socklen_t clientAddrLen) {
    // Open the file for reading
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file for reading");
        return DONE;
    }

    // Initialize the sender window
    SenderWindow *window = initSenderWindow(window_size, buffer_size);
    if (!window) {
        return DONE;
    }

    int bytesRead = 0;
    uint8_t *data_chunk = NULL;
    int doneSending = 0;

    while (!doneSending) {

        // **Step 1: Fill the window while it is open and not EOF**
        while (canSendMorePackets(window)) {
            // Read next chunk from file
            data_chunk = getDataChunk(file, buffer_size, &bytesRead);

            // **Handle EOF case**
            if (bytesRead == 0) {
                printf("Reached EOF, sending EOF packet.\n");
                //! write this !!!! sendEOFpacket(child_server_socket, client, clientAddrLen); // Call the EOF sending function
                return WAIT_ON_EOF_ACK;  // Move to EOF state immediately
            }

            // Create the Data PDU
            uint8_t *dataPDU = makeDataPacketBeforeChecksum(data_chunk, buffer_size);
            uint16_t checksum = calculateDataPacketChecksum(dataPDU, bytesRead);
            uint8_t *finalPDU = makeDataPacketAfterChecksum(dataPDU, checksum);

            // Insert into the sender window
            insertPacketIntoWindow(window, data_sequence_number, finalPDU, bytesRead + 7);

            // Send the packet to the receiver
            int index = data_sequence_number % window->window_size;
            int bytes_sent = sendtoErr(child_server_socket, window->buffer[index]->data,
                                       bytesRead + 7, 0, (struct sockaddr*) client, clientAddrLen);

            printf("Sent packet #%d, Bytes: %d\n", data_sequence_number, bytes_sent);

            if (bytes_sent < 0) {
                perror("sendtoErr failed");
                return DONE;
            }

            data_sequence_number++;  // Increment the global sequence number
        }

        // **Step 2: Poll indefinitely for RR/SREJ responses**
        while (pollCall(0) > 0) {  // Poll indefinitely (as long as there is data to process)
            uint8_t recv_buffer[ACK_BUFF_SIZE];
            struct sockaddr_in6 fromAddr;
            socklen_t fromLen = sizeof(fromAddr);

            int received = recvfrom(child_server_socket, recv_buffer, ACK_BUFF_SIZE, 0,
                                    (struct sockaddr*)&fromAddr, &fromLen);

            if (received > 0) {
                uint8_t response_flag = recv_buffer[6];  // Extract flag
                uint32_t receivedSeq = getPacketSequence((Packet*) recv_buffer);

                if (response_flag == 5) {  // RR Received
                    printf("Received RR for %d\n", receivedSeq);
                    acknowledgePacket(window, receivedSeq);
                    advanceSenderWindow(window, receivedSeq + 1);
                } else if (response_flag == 6) {  // SREJ Received
                    printf("Received SREJ for %d, resending...\n", receivedSeq);
                    int index = receivedSeq % window->window_size;
                    sendtoErr(child_server_socket, window->buffer[index]->data,
                              window->buffer[index]->data_size + 7, 0,
                              (struct sockaddr*) client, clientAddrLen);
					//!add the error check for if send bytes is < 0
                }
            }
        }

        // **Step 3: If the window is full and not EOF, wait for RR/SREJ**
		// now the window is full but the file is not over 
        while (!canSendMorePackets(window) && !doneSending) {
            int pollResult = pollCall(1000);  // Poll with 1s timeout
            if (pollResult > 0) {
                // Process RR or SREJ
                uint8_t recv_buffer[ACK_BUFF_SIZE];
                struct sockaddr_in6 fromAddr;
                socklen_t fromLen = sizeof(fromAddr);

                int received = recvfrom(child_server_socket, recv_buffer, ACK_BUFF_SIZE, 0,
                                        (struct sockaddr*)&fromAddr, &fromLen);

                if (received > 0) {
                    uint8_t response_flag = recv_buffer[6];
                    uint32_t receivedSeq = getPacketSequence((Packet*) recv_buffer);

                    if (response_flag == 5) {  // RR
                        printf("Received RR for %d\n", receivedSeq);
                        acknowledgePacket(window, receivedSeq);
                        advanceSenderWindow(window, receivedSeq + 1);
                    } else if (response_flag == 6) {  // SREJ
                        printf("Received SREJ for %d, resending...\n", receivedSeq);
                        int index = receivedSeq % window->window_size;
                        sendtoErr(child_server_socket, window->buffer[index]->data,
                                  window->buffer[index]->data_size + 7, 0,
                                  (struct sockaddr*) client, clientAddrLen);
						//!add the error check for if send bytes is < 0
                    }
                }
            } 
			else {
				// Timeout: Find the lowest unacknowledged packet in the window
				int found_unacked = 0;
				int seq;
			
				// loop through and find the lowest one that is unacknowledged and send that one
				for (seq = window->lower; seq <= window->upper; seq++) {
					int index = seq % window->window_size;
			
					// Check if the packet exists and is NOT acknowledged
					if (window->buffer[index] && window->buffer[index]->ACK == 0) {
						printf("Timeout occurred, resending unacknowledged packet with sequence number %d\n", seq);
						
						// Resend the packet
						sendtoErr(child_server_socket, window->buffer[index]->data,
								  window->buffer[index]->data_size + 7, 0,
								  (struct sockaddr*) client, clientAddrLen);
						
						found_unacked = 1; // Mark that we found an unacknowledged packet
						break; // Stop searching after resending the first unacknowledged packet
					}
				}
			
				// If all packets in the window are already acknowledged, do nothing
				if (!found_unacked) {
					printf("Timeout occurred, but all packets in the window are already acknowledged. No retransmission needed.\n");
				}
			}			
        }
    }

    return WAIT_ON_EOF_ACK; // Final transition after all packets sent
}








// will take in the chunk of data read out from the file and make the PDU
uint8_t* makeDataPacketBeforeChecksum(uint8_t* data_chunk, uint8_t buffer_size){
	uint8_t *buffer[MAX_HEADER_LEN + buffer_size];

	//chose 1 for the sequence number for acks as well
    uint32_t sequence_num = ntohl(data_sequence_number);
    memcpy(buffer, &sequence_num, 4);
	
	//initially put checksum zerod out
	uint16_t zero_checksum = 0;
    memcpy(buffer + 4, &zero_checksum, 2);

	//flag for fileame packet is 8
	uint8_t flag = 16;
    memcpy(buffer + 6, &flag, 1);

	//put the data in there
	memcpy(buffer + 7, data_chunk, buffer_size);

    return *buffer;
}

uint8_t* makeDataPacketAfterChecksum(uint8_t *buffer, uint16_t calculated_checksum){
	//put in the calculated checksum
    memcpy(buffer + 4, &calculated_checksum, 2);
    return buffer;
}

uint16_t calculateDataPacketChecksum(uint8_t *buffer, int buffer_size){
	uint8_t length_of_buffer = buffer_size + 7;
	if (length_of_buffer % 2 == 0){
		// it is already even 
		uint16_t temp_buff[buffer_size + 7];
		// copy in the header
		memcpy(temp_buff, buffer, 7);
		// copy in the data (buffer size number of bytes after the header)
		memcpy(temp_buff + 7, buffer, buffer_size);
		uint16_t calculatedChecksum = in_cksum(temp_buff, sizeof(temp_buff));
		return calculatedChecksum;
	}
	// it is odd, add 1 more byte
	uint16_t temp_buff[buffer_size + 7 + 1];
	//goal is to make the static buffer the next closest even number of bytes based on the buffer size + 7
	// copy in the header
	memcpy(temp_buff, buffer, 7);
	// copy in the data (buffer size number of bytes after the header)
	memcpy(temp_buff + 7, buffer, buffer_size);
	uint8_t even_length = 0;
	memcpy(temp_buff + 7, &even_length, 1);
	uint16_t calculatedChecksum = in_cksum(temp_buff, sizeof(temp_buff));
	return calculatedChecksum;
}

//helper function to read through the file and make data packets 
// will read through the input file in the amount of buffer_size bytes specified
	// will put that in a byte buffer
	// will pass that to the next functions
    // will use the functions above to make the data PDU
	//will return the data in a buffer
	//bytes_read stores how many bytes were read
	// check this to make sure that its not EOF
	uint8_t* getDataChunk(FILE *fp, int buffer_size, int *bytes_read)
	{
		if (!fp || buffer_size <= 0 || !bytes_read) {
			// Invalid parameters
			if (bytes_read) {
				*bytes_read = 0;
			}
			return NULL;
		}
	
		// Allocate a temporary buffer on the heap
		uint8_t *chunk = (uint8_t*)malloc(buffer_size);
		if (!chunk) {
			perror("Failed to allocate data chunk");
			*bytes_read = 0;
			return NULL;
		}
	
		// Attempt to read up to 'buffer_size' bytes
		size_t nread = fread(chunk, 1, buffer_size, fp);
	
		if (nread == 0) {
			// No data read -> either EOF or error
			free(chunk);
			*bytes_read = 0;
			return NULL;
		}
	
		// We successfully read some bytes
		*bytes_read = (int)nread;
	
		return chunk;
	}


// int doWaitOnAckState() {
// 	return 0;
// }
// int doWaitOnEOFAckState() {
// 	return 0;
// }


// error-rate and optional port number
ServerParams checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	ServerParams params;

	if (argc > 3 || argc < 3)
	{
		fprintf(stderr, "Usage %s error-rate optional-port-number\n", argv[0]);
		exit(-1);
	}
	
	//if right amount of arguments, check if the values given are valid
	if (argc == 3)
	{
		if (atoi(argv[2]) <= 0 || atof(argv[1]) < 0.0 || atof(argv[1]) > 1.0) {
        	printf("Error: Invalid values. Make sure port-number is positive and error-rate is between 0 and 1\n");
        	exit(1);
		}
		else{
			params.error_rate = atof(argv[1]);
			params.port_number = atoi(argv[2]);
		}
	}
	return params;
}


uint8_t* makeVALIDFilenameACKBeforeChecksum(){

    uint8_t static buffer[ACK_BUFF_SIZE];

	//chose 1 for the sequence number for acks as well
    uint32_t sequence_num = ntohl(1);
    memcpy(buffer, &sequence_num, 4);
	
	//initially put checksum zerod out
	uint16_t zero_checksum = 0;
    memcpy(buffer + 4, &zero_checksum, 2);

	//flag for fileame packet is 8
	uint8_t flag = 9;
    memcpy(buffer + 6, &flag, 1);

    return buffer;
}

uint8_t* makeERRORFilenameACKBeforeChecksum(){

    uint8_t static buffer[ACK_BUFF_SIZE];

	//chose 1 for the sequence number for acks as well
    uint32_t sequence_num = ntohl(1);
    memcpy(buffer, &sequence_num, 4);
	
	//initially put checksum zerod out
	uint16_t zero_checksum = 0;
    memcpy(buffer + 4, &zero_checksum, 2);

	//flag for fileame packet is 8
	uint8_t flag = 32;
    memcpy(buffer + 6, &flag, 1);

    return buffer;
}

uint16_t calculateFilenameChecksumACK(uint8_t* buffer){
	uint16_t static temp_buff[8];
	memcpy(temp_buff, buffer, 7);
	uint8_t even_length = 0;
	memcpy(temp_buff + 7, &even_length, 1);
	uint16_t calculatedChecksum = in_cksum(temp_buff, sizeof(temp_buff));
	return calculatedChecksum;
}

uint8_t* makeFilenameACKAfterChecksum(uint8_t* buffer, uint16_t calculated_checksum){

	//put in the calculated checksum
    memcpy(buffer + 4, &calculated_checksum, 2);

    return buffer;
}

uint8_t* makeTalkHereNowBeforeChecksum(){

	uint8_t static buffer[ACK_BUFF_SIZE];

	//chose 1 for the sequence number for acks as well
    uint32_t sequence_num = ntohl(1);
    memcpy(buffer, &sequence_num, 4);
	
	//initially put checksum zerod out
	uint16_t zero_checksum = 0;
    memcpy(buffer + 4, &zero_checksum, 2);

	//flag for "send here now" is 33
	uint8_t flag = 33;
    memcpy(buffer + 6, &flag, 1);

    return buffer;
}

uint8_t* makeTalkHereNowAfterChecksum(uint8_t* buffer, uint16_t calculated_checksum){

	//put in the calculated checksum
    memcpy(buffer + 4, &calculated_checksum, 2);

    return buffer;
}


// Debug print function for "Talk to Here Now" PDU
void printPDU(uint8_t *buffer) {
    uint32_t seqNum;
    uint16_t checksum;
    uint8_t flag;

    // Extract fields from buffer
    memcpy(&seqNum, buffer, 4);
    memcpy(&checksum, buffer + 4, 2);
    memcpy(&flag, buffer + 6, 1);

    // Print values
    printf("------ PDU ------\n");
    printf("Sequence Number: %u\n", ntohl(seqNum));
    printf("Checksum: 0x%04x\n", checksum);
    printf("Flag: %u\n", flag);
    printf("----------------------------------\n");
}


// SIGCHLD handler - cleans up terminated child processes
void handleZombies(int sig) {
    int stat = 0;
    while (waitpid(-1, &stat, WNOHANG) > 0) {} // Non-blocking cleanup of all finished child processes
}

// gets the sequence number out
uint32_t getPacketSequence(Packet *packet)
{
    if (!packet) {
		return 0;
	}
    uint32_t netSeq;
    memcpy(&netSeq, packet->data, 4);
    // Convert from network to host byte order
	uint32_t host_sequence_number = ntohl(netSeq);
	return host_sequence_number;
}