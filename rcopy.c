#include "rcopy.h"
typedef enum {
    SEND_FILENAME,
    FILE_VALID,
    GET_DATA,
    DONE
} state;

int main (int argc, char *argv[])
 {	
	int socketNum = 0;				
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct
	int state = SEND_FILENAME;

	setupPollSet();

	RcopyParams params = checkArgs(argc, argv);

	//! global that holds the main server upon start
	//! when we start this is where we are talking to
	main_server_port = params.remote_port;
	
	socketNum = setupUdpClientToServer(&server, params.remote_machine, main_server_port);
	printf("first send with this socketNum: %d\n", socketNum);

	//need to call this here bcasue need to call this before calling sendErr?
	sendErr_init(params.error_rate, DROP_ON, FLIP_OFF, DEBUG_OFF, RSEED_OFF);

	uint8_t* buffer = makeFilenamePDUBeforeChecksum(params);
	uint16_t calculated_checksum = calculateFilenameChecksum(buffer);
	uint8_t* buffer_to_send = makeFilenamePDUAfterChecksum(buffer, calculated_checksum);

	//! DEBUG: will recalculate the checksum after it has been calculated before sending 
	uint16_t result = calculateFilenameChecksum(buffer_to_send);
	if (result == 0){
		printf("checksum calculated right: %d\n", result);
	}
	else{
		printf("checksum not right\n");
		exit(1);
	}

	printFilenamePDU(buffer_to_send);

	//while the state is not the done state
    while (state != DONE) {
		//check the state
        switch (state) {
            case SEND_FILENAME:
                state = doSendFilenameState(params, socketNum, &server, buffer_to_send);
                break;
            case FILE_VALID:
                printf("checking if file can be opened\n");
                state = doFileValidState(params, socketNum, &server, buffer_to_send);
                break;
            case GET_DATA:
                printf("heyyyyy can get data now!!!");
                state = doGetDataState(params);
                break;
            case DONE:
                printf("h");
				doDoneState(socketNum, curr_file_open);
                break;
        }
    }
	close(socketNum);
	return 0;
}


int doSendFilenameState(RcopyParams params, int socketNum, struct sockaddr_in6 * server, uint8_t* buffer){
	socklen_t serverAddrLen = sizeof(struct sockaddr_in6);
	uint8_t recv_buffer[ACK_BUFF_SIZE];
	int sends_attempted = 0;
	int timeout = 1000; //need it to be one second 
	int poll; //make a socket to recieve on ? 

	printf("WHAT IS THE CURRENT SERVER PORT: %d\n", main_server_port);

	// add the current socket to the poll set
	addToPollSet(socketNum);

	// can send up to 10 times
	while (sends_attempted < 9){
		printf("sending filename packet...\n");
		printf("the buffer that is being sent: ");
		int i;
    	for (i = 0; i < 114; i++) {
        	printf("%02X ", buffer[i]);
    	}
    	printf("hfgh\n");
		int bytes_sent = sendtoErr(socketNum, buffer, 114, 0, (struct sockaddr *) server, serverAddrLen);
		if (bytes_sent < 0){
			perror("sendtoErr failed\n");
			removeFromPollSet(socketNum);
            close(socketNum);
			return DONE;
		}


		struct sockaddr_in6 client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		getsockname(socketNum, (struct sockaddr *)&client_addr, &client_addr_len);

		char client_ip_str[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &client_addr.sin6_addr, client_ip_str, sizeof(client_ip_str));

		printf("Client is listening on IP FOR 33 PACKET: %s, Port: %d\n", client_ip_str, ntohs(client_addr.sin6_port));

 
		poll = pollCall(timeout);
		if (poll == -1){
			//that means the timeout expired so need to resend 
			//close the current socket
			//open a new socket, send on the new one

			sends_attempted++;
			continue;
		}
		// now the case that the timer has NOT expired
		// it returned a valid socket to read so its going to recieve data
		// after it receives it and the flag is the filename ack flag it will go to get data state
		else {
			int recieved = recvfrom(socketNum, recv_buffer, ACK_BUFF_SIZE, 0, (struct sockaddr *) server, &serverAddrLen);
			printf("RECEIVING 33 port number: %d\n", ntohs(server->sin6_port));
			char server_ip_str[INET6_ADDRSTRLEN];  // Buffer to store the IP string
			inet_ntop(AF_INET6, &server->sin6_addr, server_ip_str, sizeof(server_ip_str));  // Convert binary IP to strin
			//printf("RECEIVING 33 address : %d\n", server_ip_str);
			printf("RECEIVING 33 sending socket number: %d\n", socketNum);

			printf("Client is receiving on: %d\n", socketNum);
			if (recieved < 0) {
            	perror("recvfrom failed");
				removeFromPollSet(socketNum);
            	close(socketNum);
            	return DONE;
        	}
			//check if it was a filename ack
			// the flag should be byte 7 of the buffer
        	uint8_t response_flag = recv_buffer[6];
			printf("WHAT FLAG ARE WE GETTING??: %d\n", response_flag);

			// if the response is a "talk to here now" packet
			// stop talking to the main server and redirect all communication to this new port
			// reset the poll
			// reset the counter
			if (response_flag == 33) { 

				//! update the global variable
				current_server_port = ntohs(server->sin6_port);
            	server->sin6_port = htons(current_server_port);

				printf("WHAT IS THE CURRENT SERVER PORT: %d\n", current_server_port);

				printf("got the 'talk to here now' packet. redirecting to child server\n");
				printf("New child server port received: %d\n", current_server_port);

				// it now knows where to communicate to. move onto getting data from there
				return FILE_VALID;
        	} 
			else {
				// if the first response from the server isnt telling me to talk to a child, something is wrong and abort
            	removeFromPollSet(socketNum);
				close(socketNum);
            	return DONE;
        	}
    	}
	}
	//the case where the sends_attempted are greater than 9
	removeFromPollSet(socketNum);
	close(socketNum);
	return DONE;
}

//choosing to handle the ack packets from the child server in here
// this function will also check if the destination file can be opened or not? 
int doFileValidState(RcopyParams params, int socketNum, struct sockaddr_in6 * server, uint8_t* buffer){

	//socklen_t serverAddrLen = sizeof(struct sockaddr_in6);
	uint8_t recv_buffer[ACK_BUFF_SIZE];

	//first check if the destination file to write to can be opened
	int result = checkDestFile(params);
	if (result == 0){
		return DONE;
	}

	//now want to poll and wait for the server to send the ack packet
	//keep polling for the ack packets
	int new_poll = 1;
	int retries = 0; 
	int timeout = 1000;
	

	while (retries < 9){
		printf("new_poll before pollCall: %d\n", new_poll);


		struct sockaddr_in6 client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		getsockname(socketNum, (struct sockaddr *)&client_addr, &client_addr_len);

		char client_ip_str[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &client_addr.sin6_addr, client_ip_str, sizeof(client_ip_str));

		printf("Client is listening on IP: %s, Port: %d\n", client_ip_str, ntohs(client_addr.sin6_port));



		new_poll = pollCall(timeout);
		printf("new_poll after pollCall: %d\n", new_poll);
		if (new_poll != -1){
			printf("polling again\n");
			socklen_t serverAddrLen = sizeof(*server);

			int second_recieved = recvfrom(socketNum, recv_buffer, ACK_BUFF_SIZE, 0, (struct sockaddr *) server, &serverAddrLen);

			if (second_recieved < 0) {
            	perror("recvfrom failed");
				//! do i end here? what do I do? 
				continue;
        	}

			//check if it was a filename ack
			// the flag should be byte 7 of the buffer
       		uint8_t response_flag = recv_buffer[6];
			printf("WHAT FLAG ARE WE GETTING NOWWW??: %d\n", response_flag);


			//now wait on the server child to try and open the file and send an ack packet
        	if (response_flag == 9) {
				printf("file was openable\n");
            	return GET_DATA;
        	} 
        	else if (response_flag == 32) {
            printf("File is NOT valid. Sending ACK and terminating.\n");

            // Create ACK packet using existing helper functions
            uint8_t* ack_packet = makeFilenameACKBeforeChecksum(33);  // Use flag 33 for ACK
            uint16_t calculated_checksum = calculateFilenameChecksumACK(ack_packet);
            uint8_t* ack_packet_to_send = makeFilenameACKAfterChecksum(ack_packet, calculated_checksum);

            // Send the ACK to server
            int bytes_sent = sendtoErr(socketNum, ack_packet_to_send, ACK_BUFF_SIZE, 0, 
                                       (struct sockaddr *)server, serverAddrLen);
            if (bytes_sent < 0) {
                perror("Failed to send ACK for File Not OK\n");
            } else {
                printf("Sent ACK for File Not OK to server on port: %d\n", ntohs(server->sin6_port));
            }

			printf("File could not be opened... terminating client\n");
            removeFromPollSet(socketNum);
            return DONE;
        }
    }
	retries++;
	}
	printf("Max retries reached. Resending filename.\n");
    return SEND_FILENAME;
}

int checkDestFile(RcopyParams params){
	FILE* file_opened = fopen(params.dest_filename, "wb");

	if (file_opened == NULL){
		perror("Couldn't open file\n");
		return 0;
	}

	return 1;
}


int doGetDataState(int socketNum, struct sockaddr_in6 *server, RcopyParams params){

	static ReceiveState subState = IN_ORDER;  //this is the starting state (we expect things to come in order)
    int doneReceiving = 0;
	int attempts = 0;

	//open the destination file for writing bytes to it
	//if the destination file can't be opened here then go to DONE
    FILE *fp = fopen(params.dest_filename, "ab");
    if (!fp) {
        perror("Failed to open output file in GET_DATA");
        return DONE; // or handle error
    }

	 // initialize the buffer and make sure it got initialized properly
	 ReceiverBuffer* rbuf = initReceiverBuffer(params.buffer_size, params.window_size);
	 if (!rbuf) {
		 fprintf(stderr, "Error: could not allocate receiver buffer.\n");
		 fclose(fp);
		 return DONE;
	 }

	 // while I am still receiving packets 
	 while (!doneReceiving) {
        // keep checking for incoming data
        int pollResult = pollCall(1000); // set a one second timeout 
        if (pollResult > 0) {
            Packet incomingPkt;
            struct sockaddr_in6 fromAddr;
            socklen_t fromLen = sizeof(fromAddr);

            //receive the data
            int bytes = recvfrom(socketNum, &incomingPkt, sizeof(incomingPkt), 0,
                                 (struct sockaddr*)&fromAddr, &fromLen);

            if (bytes <= 0) {
                //!check if nothing was recieved, then move to DONE? 
                return DONE;
            }

            //start up the second state machine
            subState = handleDataPacket(rbuf, &incomingPkt, fp, subState, socketNum, server, params);

            // Check for EOF
            // extract the flag and make sure that its not 10
            uint8_t flag = getPacketFlag(&incomingPkt);
            if (flag == 10) {
				// if it is EOF, then move to DONE state
				//! write the EOF to the output file? 
                //!Then send an EOF ACK if your protocol requires it
                //!make and send an EOF ACK packet, not sure about this
                doneReceiving = 1;
				return DONE;
            }
        }
        else {
			// else if it times out, increment the attempts then check if it has been over 9 tries
			// if it has then leave the program? 
			attempts++;

			if (attempts > 9){
				//! must ask about this
				return DONE;
			}
			//! this means nothing came and it timed out
            fprintf(stderr, "Timeout occurred in GET_DATA; consider re-sending SREJ or abort.\n");
     		doneReceiving = 1;
        }
    }

	// at the end of the state, clean that shit up and close the end file
	//! should I do this after the EOF is dealt with? what if that gets lost? 
    freeReceiverBuffer(rbuf);
    fclose(fp);
    return DONE;
}


ReceiveState handleDataPacket(ReceiverBuffer *rbuf, Packet *incomingPkt, FILE *fp, ReceiveState currentState, int socketNum, struct sockaddr_in6 *server,
	RcopyParams params)
{
	uint32_t seqIncoming = getPacketSequence(incomingPkt);

	// --- Step 1: Check if seqIncoming < expected (old packet) ---
	if (seqIncoming < rbuf->expected) {
	// Resend the last control packet we sent (RR or SREJ).
		resendLastControl(rbuf, socketNum, server);
		// Remain in the same state, effectively ignoring the old packet.
		return currentState;
	}

	// --- Step 2: If seqIncoming >= expected, proceed with normal sub-state logic ---
	switch (currentState) {
		case IN_ORDER:
			return doInOrderState(rbuf, incomingPkt, fp, socketNum, server, params);
		case BUFFERING:
			return doBufferState(rbuf, incomingPkt, fp, socketNum, server, params);
  		case FLUSHING:
			return doFlushingState(rbuf, fp, socketNum, server, params);
	}
	return currentState;
}



//should take in the incoming packet, the buffer struct, and destination file
	// set the expected counter at 0
	// parse the received packet 
	// check if the expected packet matches the sequence number of the received packet 
	// call makeRRpacket
	// send the RR packet for the packet just received 
	// increment expected
	// if the received sequence number > expected number, move to buffering state
	// else just stay in this state.
	ReceiveState doInOrderState(ReceiverBuffer *rbuf, Packet *incomingPkt, FILE *fp, int socketNum, struct sockaddr_in6 *server, RcopyParams params)
{
	//the next expected
	uint32_t seqExpected = rbuf->expected;
	//actual seq of the incoming packet
	uint32_t seqIncoming = getPacketSequence(incomingPkt);

	if (seqIncoming == seqExpected) {
		// if its what we want write data directly to output file
		writePayloadToFile(incomingPkt, fp, params);

		//increment the expected
		rbuf->expected++;

		// Send RR for the new expected
		// localSeq could be anything. For example, a local control sequence counter.
		// Or you can reuse 'seqExpected' if you want.
		uint8_t *rrBefore = makeRRPacketBeforeChecksum(rr_seq_num, rbuf->expected);
		uint16_t rrCsum = calculateRR_SREJPacketChecksum(rrBefore);
		uint8_t* finalRRBuff = makeRR_SREJPacketAfterChecksum(rrBefore, rrCsum);

		// Send the 11-byte RR
		memcpy(rbuf->lastControlPacket, finalRRBuff, CONTROL_BUFF_SIZE);
    	rbuf->lastControlPacketSize = CONTROL_BUFF_SIZE;
    	rbuf->hasLastControl = 1;

		int retries = 0;
		while (retries < 9){
			int bytes_sent = sendtoErr(socketNum, finalRRBuff, CONTROL_BUFF_SIZE, 0, (struct sockaddr *)server, sizeof(*server));
			if (bytes_sent >= 0){
				break;
			}
			perror("sending RR failed\n");
			retries++;
			rr_seq_num++;

			// Poll for incoming packets before retrying
            int pollResult = pollCall(1000);  // Wait for 1 second
            if (pollResult == socketNum) {
                // Received something, check if it's the next expected packet
                Packet responsePkt;
                int received_bytes = recvfrom(socketNum, &responsePkt, sizeof(responsePkt), 0, NULL, NULL);
                if (received_bytes > 0) {
                    uint32_t receivedSeq = getPacketSequence(&responsePkt);
                    uint8_t receivedFlag = getPacketFlag(&responsePkt);

                    if (receivedSeq == rbuf->expected && receivedFlag == 16) { // 16 = DATA_PACKET_FLAG
                        // We got the next expected packet → Stay in IN_ORDER
                        return IN_ORDER;
					}

				}
			}
		}

        // If we fail after 10 tries, move to BUFFERING
		printf("tried resending 10 times, server not responsive");
        return DONE;
    } 
    else if (seqIncoming > seqExpected) {
        // If packet is out-of-order, move to BUFFERING
        return BUFFERING;
    } 
    else {
        // If packet is old/duplicate, ignore it and stay in IN_ORDER
        return IN_ORDER;
    }
}

void resendLastControl(ReceiverBuffer *rbuf, int socketNum, struct sockaddr_in6 *server)
{
    if (rbuf->hasLastControl) {
        // re-send the exact bytes
        int bytesSent = sendtoErr(socketNum, rbuf->lastControlPacket,
                                  rbuf->lastControlPacketSize, 0,
                                  (struct sockaddr*)server, sizeof(*server));
        if (bytesSent < 0) {
            perror("resendLastControl failed");
        }
    } else {
        //! not sure what to do here!
		printf("reached here and don't know what to do");
    }
}


	// while the received sequence number > expected number
	// put the packet into the buffer
	// set the highest variable to the received sequence number
	// will tell you the largest thing in the buffer
	// use that to know what to send an RR for after the buffer is flushed (what is next)
	//  if th received packet sequence number == the expeced number move to the flushing state

ReceiveState doBufferState(ReceiverBuffer *rbuf, Packet *incomingPkt, FILE *fp, int socketNum, struct sockaddr_in6 *server, RcopyParams params)
{
	// which is the next expected
    uint32_t seqExpected = rbuf->expected;
    uint32_t seqIncoming = getPacketSequence(incomingPkt);

    // Store the out-of-order packet in the buffer
    insertReceiverPacket(rbuf, incomingPkt);
    
    // Update highest received packet
    if (seqIncoming > rbuf->highest) {
        rbuf->highest = seqIncoming;
    }

    // Retry sending SREJ up to 10 times
    int retries = 0;
    while (retries < 10) {
        // Send an SREJ for the missing packet
        uint8_t *srejBefore = makeSREJPacketBeforeChecksum(srej_seq_num, seqExpected);
        uint16_t srejCsum = calculateRR_SREJPacketChecksum(srejBefore);
        uint8_t* buff_to_send = makeRR_SREJPacketAfterChecksum(srejBefore, srejCsum);

        int sent_bytes = sendtoErr(socketNum, buff_to_send, CONTROL_BUFF_SIZE, 0, (struct sockaddr *)server, sizeof(*server));
        if (sent_bytes >= 0) {
            printf("SREJ sent for missing packet %u\n", seqExpected);
        } else {
            perror("sending SREJ failed, retrying...");
        }

        srej_seq_num++;

        // Poll for the missing packet before retrying
        int pollResult = pollCall(1000);  // 1-second timeout
        if (pollResult == socketNum) {
            // Received something, check if it's the missing packet
            Packet responsePkt;
            int received_bytes = recvfrom(socketNum, &responsePkt, sizeof(responsePkt), 0, NULL, NULL);
            if (received_bytes > 0) {
                uint32_t receivedSeq = getPacketSequence(&responsePkt);
                uint8_t receivedFlag = getPacketFlag(&responsePkt);

                if (receivedSeq == seqExpected && receivedFlag == 16) { // 16 = DATA_PACKET_FLAG
                    // The missing packet has arrived → Move to FLUSHING
                    return FLUSHING;
                }
            }
        }
        retries++;
    }

    // If after 10 retries the expected packet never comes, assume sender is dead
    printf("ERROR: SREJ sent 10 times, but missing packet %u never arrived. Terminating...\n", seqExpected);
    return DONE;
}



	// write the expected and received packet to disc 
	// increment the expected count 
	// while the sequence number == the expected number 
	// flush out the buffer
	// if flushing ends and the buffer is not empty, return to Buffering State
		// send an RR for the highest # packet before moving to next state
		// send an SREJ for the expected. 
		// flushing will stop when it encounters something that is not valid
	// if flushing ends and the buffer is empty, then more to the in order state again (expected = highest)
		// increment expected
		// RR for the expected

		ReceiveState doFlushingState(ReceiverBuffer *rbuf, FILE *fp, int socketNum, struct sockaddr_in6 *server, RcopyParams params)
		{
			printf("Entering FLUSHING state. Expected: %d, Highest: %d\n", rbuf->expected, rbuf->highest);
		
			// Call `flushBuffer()` to write packets to the file
			int flushedPackets = flushBuffer(rbuf, fp, params.buffer_size);
		
			if (flushedPackets > 0) {
				printf("Flushed %d packets to file. New expected: %d\n", flushedPackets, rbuf->expected);
			} else {
				printf("No packets flushed. Possible missing packet at %d.\n", rbuf->expected);
			}
		
			printReceiverBuffer(rbuf); // Debugging
		
			if (rbuf->expected > rbuf->highest) {
				// Buffer is empty, transition to IN_ORDER and send an RR
				printf("Buffer fully flushed. Moving to IN_ORDER.\n");
				return IN_ORDER;
			} else {
				// Missing packet detected, move to BUFFERING and send an SREJ
				printf("Flushing stopped at missing packet %d. Moving to BUFFERING.\n", rbuf->expected);
				return BUFFERING;
			}
		}


void doDoneState(int socketNum, FILE *destFile){
	
	//if the file was open close it
    if (destFile != NULL) {
        fclose(destFile);
    }

    // Remove from poll set and close socket
    removeFromPollSet(socketNum);
    close(socketNum);

	printf("Error somemwhere previously...terminating client.\n");
	exit(1);
}

//debug print function 
void printFilenamePDU(uint8_t *buffer) {
    uint32_t seqNum;
    uint16_t checksum;
    uint8_t flag;
    char filename[101]; // +1 for null terminator
    uint16_t bufferSize;
    uint32_t windowSize;

    // Extract fields from buffer
    memcpy(&seqNum, buffer, 4);
    memcpy(&checksum, buffer + 4, 2);
    memcpy(&flag, buffer + 6, 1);
    memcpy(filename, buffer + 7, 100);
    filename[100] = '\0'; // Ensure null-termination
    memcpy(&bufferSize, buffer + 107, 2);
    memcpy(&windowSize, buffer + 109, 4);

    // Print values
    printf("------ Filename PDU ------\n");
    printf("Sequence Number: %u\n", ntohl(seqNum));
    printf("Checksum: 0x%04x\n", checksum);
    printf("Flag: %u\n", flag);
    printf("Filename: %s\n", filename);
    printf("Buffer Size: %u\n", ntohs(bufferSize));
    printf("Window Size: %u\n", ntohl(windowSize));
    printf("--------------------------\n");
}

//will make the fist filename PDU [sequence number][checksum][flag][filename][buffer size][window size]
uint8_t* makeFilenamePDUBeforeChecksum(RcopyParams params){

    uint8_t static buffer[MAX_HEADER_LEN + MAX_FILENAME_LEN + MAX_BUFFER_LEN + MAX_WINDOW_LEN];

	//chose 1 for the sequence number
    uint32_t sequence_num = ntohl(1);
    memcpy(buffer, &sequence_num, 4);
	
	//initially put checksum zerod out
	uint16_t zero_checksum = 0;
    memcpy(buffer + 4, &zero_checksum, 2);

	//flag for fileame packet is 8
	uint8_t flag = 8;
    memcpy(buffer + 6, &flag, 1);

	memcpy(buffer + 7, &params.src_filename, 100);

	uint16_t netowrk_buffer_size = htons(params.buffer_size);

	memcpy(buffer + 107, &netowrk_buffer_size, 2);

	uint32_t netowrk_window_size = htonl(params.window_size);

	memcpy(buffer + 109, &netowrk_window_size, 4);

    return buffer;
}

uint16_t calculateFilenameChecksum(uint8_t* buffer){
	uint16_t static temp_buff[MAX_HEADER_LEN + MAX_FILENAME_LEN + MAX_BUFFER_LEN + MAX_WINDOW_LEN + 1];
	memcpy(temp_buff, buffer, 113);
	uint8_t even_length = 0;
	memcpy(temp_buff + 113, &even_length, 1);
	uint16_t calculatedChecksum = in_cksum(temp_buff, sizeof(temp_buff));
	return calculatedChecksum;
}

uint8_t* makeFilenamePDUAfterChecksum(uint8_t* buffer, uint16_t calculated_checksum){

	//put in the calculated checksum
    memcpy(buffer + 4, &calculated_checksum, 2);

    return buffer;
}

// checks the arguments to the client program and 
// src filename, dest filename, window size, buffer size, error-rate, remote-machine, remote-port. 
RcopyParams checkArgs(int argc, char * argv[])
{
	
	RcopyParams parameters;

    // check if more command line arguments than allowed
	if (argc != 8)
	{
		printf("usage: %s src-filename dest-filename window-size buffer-size error-rate remote-machine remote-port \n", argv[0]);
		exit(1);
	}

	// make sure filenames are not above 100 characters 
	if (strlen(argv[1]) > MAX_FILENAME_LEN || strlen(argv[1]) == 0 || strlen(argv[2]) > MAX_FILENAME_LEN || strlen(argv[2]) == 0) {
        printf("Error: Filenames must be between 1 and %d characters.\n", MAX_FILENAME_LEN);
        exit(1);
    }

    if (atoi(argv[3]) <= 0 || atoi(argv[4]) <= 0 || atoi(argv[7]) <= 0 ||
        atof(argv[5]) < 0.0 || atof(argv[5]) > 1.0) {
        printf("Error: Invalid values. Make sure window-size, buffer-size, and port-number are positive and error-rate is between 0 and 1\n");
        exit(1);
	}

	// if they are valid, then put them into the struct

	strncpy(parameters.src_filename, argv[1], MAX_FILENAME_LEN);
    parameters.src_filename[MAX_FILENAME_LEN] = '\0';

	strncpy(parameters.dest_filename, argv[1], MAX_FILENAME_LEN);
    parameters.dest_filename[MAX_FILENAME_LEN] = '\0';

	parameters.window_size = atoi(argv[3]);
	parameters.buffer_size = atoi(argv[4]);
	parameters.error_rate = atof(argv[5]);

	strncpy(parameters.remote_machine, argv[6], MAX_REMOTE_MACHINE);
    parameters.remote_machine[MAX_REMOTE_MACHINE] = '\0';

	parameters.remote_port = atoi(argv[7]);

	printf("Parsed Arguments:\n");
    printf("  Source File: %s\n", parameters.src_filename);
    printf("  Destination File: %s\n", parameters.dest_filename);
    printf("  Window Size: %d packets\n", parameters.window_size);
    printf("  Buffer Size: %d bytes\n", parameters.buffer_size);
    printf("  Error Rate: %.2f\n", parameters.error_rate);
    printf("  Remote Machine: %s\n", parameters.remote_machine);
    printf("  Remote Port: %d\n", parameters.remote_port);

	return parameters;
}


uint8_t* makeFilenameACKBeforeChecksum(uint8_t flag){

    uint8_t static buffer[ACK_BUFF_SIZE];

	//chose 1 for the sequence number for acks as well
    uint32_t sequence_num = ntohl(1);
    memcpy(buffer, &sequence_num, 4);
	
	//initially put checksum zerod out
	uint16_t zero_checksum = 0;
    memcpy(buffer + 4, &zero_checksum, 2);

	//put the right flag in there
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

int readFromStdin(char * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("Enter data: ");
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

// helper

//will take the packet input and write just the data part to the output file 
void writePayloadToFile(Packet *packet, FILE *file, RcopyParams params)
{
	// if the packet is NULL or the file is not open or something then return
    if (!packet || !file) {
		return;
	}
	//how much data there will be is specified in the command line arguments as buffer_size 
    int payloadSize = params.buffer_size;
    if (payloadSize <= 0) {
        // No payload or invalid size
        return;
    }

    // Write from data[7..end]
    fwrite(packet->data + HEADER_SIZE, 1, payloadSize, file);
}

// gets just the flag out so I can check what the flag is 
uint8_t getPacketFlag(Packet *packet)
{
    if (!packet) {
		return 0;
	} 
    // The flag is at data[6]
    return packet->data[6];
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


uint8_t* makeRRPacketBeforeChecksum(uint32_t localSeq, uint32_t rrSequence)
{
    static uint8_t buffer[CONTROL_BUFF_SIZE];

    // 1) Convert localSeq to network order, copy into first 4 bytes
    uint32_t netLocalSeq = htonl(localSeq);
    memcpy(buffer, &netLocalSeq, 4);

    // 2) Zero out the checksum (2 bytes)
    uint16_t zeroCsum = 0;
    memcpy(buffer + 4, &zeroCsum, 2);

    // 3) Set flag = 5 for RR
    uint8_t flag = 5;
    memcpy(buffer + 6, &flag, 1);

    // 4) The sequence we are acknowledging (rrSequence) in network order
    uint32_t netRR = htonl(rrSequence);
    memcpy(buffer + 7, &netRR, 4);

    return buffer;
}

uint8_t* makeSREJPacketBeforeChecksum(uint32_t localSeq, uint32_t missingPacket)
{
    static uint8_t buffer[CONTROL_BUFF_SIZE];

    // 1) Convert localSeq to network order, copy into first 4 bytes
    uint32_t netLocalSeq = htonl(localSeq);
    memcpy(buffer, &netLocalSeq, 4);

    // 2) Zero out the checksum (2 bytes)
    uint16_t zeroCsum = 0;
    memcpy(buffer + 4, &zeroCsum, 2);

    // 3) Set flag = 5 for SREJ
    uint8_t flag = 6;
    memcpy(buffer + 6, &flag, 1);

    // 4) The sequence we are acknowledging (rrSequence) in network order
    uint32_t netMissing= htonl(missingPacket);
    memcpy(buffer + 7, &netMissing, 4);

    return buffer;
}

uint16_t calculateRR_SREJPacketChecksum(uint8_t *buffer)
{
    uint16_t static temp_buff[CONTROL_BUFF_SIZE + 1];
	memcpy(temp_buff, buffer, 11);
	uint8_t even_length = 0;
	memcpy(temp_buff + 11, &even_length, 1);
	uint16_t calculatedChecksum = in_cksum(temp_buff, sizeof(temp_buff));
	return calculatedChecksum;
}

uint8_t* makeRR_SREJPacketAfterChecksum(uint8_t *buffer, uint16_t calculated_sum)
{
    memcpy(buffer + 4, &calculated_sum, 2);

    return buffer;
}