/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include "server.h"

typedef enum {
	INIT,
    GET_FILENAME,
    DONE,
    // SEND_DATA,
    // WAIT_ON_ACK,
	// WAIT_ON_EOF_ACK
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
        return 0;
    }

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

		startFSM(filename, host_buffer_size, host_window_size, &client, clientAddrLen, main_server_socket);
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


void startFSM(char* filename, uint16_t buffer_size, uint32_t window_size, struct sockaddr_in6 * client, socklen_t clientAddrLen, int main_server_socket){
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

		printf("[CHILD] New child process started with PID: %d\n", getpid());

		close(main_server_socket); 
			
		// Open a new socket for communication with this client
		//os will give it a random one
		int child_server_socket = udpServerSetup(0);

		//send the "talk to here now" packet to the client
		uint8_t* temp_buff = makeTalkHereNowBeforeChecksum();
		uint16_t calculated_checksum = calculateFilenameChecksumACK(temp_buff);
		uint8_t* talk_buff_send = makeTalkHereNowAfterChecksum(temp_buff, calculated_checksum);

		printTalkToHerePDU(talk_buff_send);

		uint16_t result = calculateFilenameChecksumACK(talk_buff_send);
		if (result == 0){
			printf("checksum calculated right: %d\n", result);
		}
		else{
			printf("checksum not right\n");
			exit(1);
		}

		// Send the "talk to here" packet
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
	}

	//while the state is not the done state
    while (state != DONE) {
		//check the state
        switch (state) {
            case GET_FILENAME:
				printf("made it to the opening file state\n");
				printf("ok now in here I can try and open the file and send the ack packet");
                state = doGetFilenameState(client, clientAddrLen, main_server_socket);
                break;
            case DONE:
                printf("DONE\n");
                state = doDoneState();
                break;
            // case SEND_DATA:
            //     printf("send data\n");
            //     //state = doSendDataState();
            //     break;
            // case WAIT_ON_ACK:
            //     printf("waiting on ack\n");
			// 	//state = doWaitOnAckState();
            //     break;
			// case WAIT_ON_EOF_ACK:
            //     printf("waiting on eof ack\n");
			// 	//state = doWaitOnEOFAckState();
            //     break;
        }
    }
}




int doGetFilenameState(struct sockaddr_in6* client, socklen_t clientAddrLen, int main_server_socket) {

	//re-read out the filename packet
	char filename[MAX_FILENAME_LEN];
	uint16_t buffer_size = 0;
	uint32_t window_size = 0;

	uint8_t buffer[114];	  
	//struct sockaddr_in6 client;		
	//socklen_t clientAddrLen = sizeof(struct sockaddr_in6);

	//receive the filename packet again from the server
	//take in the filename packet from the client
    int recv_len = recvfrom(main_server_socket, buffer, 114, 0, (struct sockaddr*)&client, &clientAddrLen);
    if (recv_len < 0) {
        perror("recvfrom failed");
        return 0;
    }

	//calculate the checksum and see if its valid 
	uint16_t result = calculateFilenameChecksumFILENAME(buffer);
	printf("result from checksum: %d\n", result);
	if (result != 0){
		printf("child bad checksum\n");
		return DONE;
	}
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
		
	return DONE;

	}

	// // try and open the file given, if possible then send back the yes ack packet and move to sending data
	// // if not, send back the no ack packet and move to DONE
	// curr_file_open = fopen(filename, "rb");

	// if (curr_file_open == NULL){
	// 	perror("Couldn't open file\n");

	// 	// make the packet for the correct ack
	// 	uint8_t temp_buff = makeVALIDFilenameACKBeforeChecksum();
	// 	uint16_t calculated_checksum = calculateFilenameChecksum(temp_buff);
	// 	uint8_t ack_buff_send = makeFilenameACKAfterChecksum(temp_buff, calculated_checksum);

	// 	uint16_t result = calculateFilenameChecksum(buffer_to_send);
	// 	if (result == 0){
	// 		printf("checksum calculated right: %d\n", result);
	// 	}
	// 	else{
	// 		printf("checksum not right\n");
	// 		exit(1);
	// 	}

	// 	//!send the packet

	// 	// must send error ack packet 


// 	// else send the right ack packet 
// 	// send back the flag 32 error ack packet
// 	printf("checksum not right\n");
// 	// make the packet for the correct ack
// 	uint8_t temp_buff = makeERRORFilenameACKBeforeChecksum();
// 	uint16_t calculated_checksum = calculateFilenameChecksum(temp_buff);
// 	uint8_t ack_buff_send = makeFilenameACKAfterChecksum(temp_buff, calculated_checksum);

// 	uint16_t result = calculateFilenameChecksum(buffer_to_send);
// 	if (result == 0){
// 		printf("checksum calculated right: %d\n", result);
// 	}
// 	else{
// 		printf("checksum not right\n");
// 		exit(1);
// 	}

// 	//! send the packet

// 	return SEND_DATA_STATE

// }


int doDoneState() {
	exit(1);
}

// int doSendDataState() {
// 	return 0;
// }
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


// uint8_t* makeVALIDFilenameACKBeforeChecksum(){

//     uint8_t static buffer[ACK_BUFF_SIZE];

// 	//chose 1 for the sequence number for acks as well
//     uint32_t sequence_num = ntohl(1);
//     memcpy(buffer, &sequence_num, 4);
	
// 	//initially put checksum zerod out
// 	uint16_t zero_checksum = 0;
//     memcpy(buffer + 4, &zero_checksum, 2);

// 	//flag for fileame packet is 8
// 	uint8_t flag = 9;
//     memcpy(buffer + 6, &flag, 1);

//     return buffer;
// }

// uint8_t* makeERRORFilenameACKBeforeChecksum(){

//     uint8_t static buffer[ACK_BUFF_SIZE];

// 	//chose 1 for the sequence number for acks as well
//     uint32_t sequence_num = ntohl(1);
//     memcpy(buffer, &sequence_num, 4);
	
// 	//initially put checksum zerod out
// 	uint16_t zero_checksum = 0;
//     memcpy(buffer + 4, &zero_checksum, 2);

// 	//flag for fileame packet is 8
// 	uint8_t flag = 32;
//     memcpy(buffer + 6, &flag, 1);

//     return buffer;
// }

uint16_t calculateFilenameChecksumACK(uint8_t* buffer){
	uint16_t static temp_buff[8];
	memcpy(temp_buff, buffer, 7);
	uint8_t even_length = 0;
	memcpy(temp_buff + 7, &even_length, 1);
	uint16_t calculatedChecksum = in_cksum(temp_buff, sizeof(temp_buff));
	return calculatedChecksum;
}

// uint8_t* makeFilenameACKAfterChecksum(uint8_t* buffer, uint16_t calculated_checksum){

// 	//put in the calculated checksum
//     memcpy(buffer + 4, &calculated_checksum, 2);

//     return buffer;
// }

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



// void processClient(int socketNum)
// {
// 	int dataLen = 0; 
// 	char buffer[MAXBUF + 1];	  
// 	struct sockaddr_in6 client;		
// 	int clientAddrLen = sizeof(client);	
	
// 	buffer[0] = '\0';
// 	while (buffer[0] != '.')
// 	{
// 		dataLen = safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *) &client, &clientAddrLen);
	
// 		printf("Received message from client with ");
// 		printIPInfo(&client);
// 		printf(" Len: %d \'%s\'\n", dataLen, buffer);

// 		// just for fun send back to client number of bytes received
// 		sprintf(buffer, "bytes: %d", dataLen);
// 		safeSendto(socketNum, buffer, strlen(buffer)+1, 0, (struct sockaddr *) & client, clientAddrLen);

// 	}
// }

// Debug print function for "Talk to Here Now" PDU
void printTalkToHerePDU(uint8_t *buffer) {
    uint32_t seqNum;
    uint16_t checksum;
    uint8_t flag;

    // Extract fields from buffer
    memcpy(&seqNum, buffer, 4);
    memcpy(&checksum, buffer + 4, 2);
    memcpy(&flag, buffer + 6, 1);

    // Print values
    printf("------ Talk to Here Now PDU ------\n");
    printf("Sequence Number: %u\n", ntohl(seqNum));
    printf("Checksum: 0x%04x\n", checksum);
    printf("Flag: %u\n", flag);
    printf("----------------------------------\n");
}
