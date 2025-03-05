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
	current_server_port = params.remote_port;
	
	socketNum = setupUdpClientToServer(&server, params.remote_machine, current_server_port);
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
                printf("h");
                state = doFileValidState(params);
                break;
            case GET_DATA:
                printf("h");
                state = doGetDataState();
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

	printf("WHAT IS THE CURRENT SERVER PORT: %d\n", current_server_port);

	// add the current socket to the poll set
	addToPollSet(socketNum);

	// can send up to 10 times
	while (sends_attempted < 9){
		printf("sending filename packet...\n");
		int bytes_sent = sendtoErr(socketNum, buffer, 114, 0, (struct sockaddr *) server, serverAddrLen);
		if (bytes_sent < 0){
			perror("sendtoErr failed\n");
			removeFromPollSet(socketNum);
            close(socketNum);
			return DONE;
		}
 
		poll = pollCall(timeout);
		if (poll == -1){
			//that means the timeout expired so need to resend 
			//close the current socket
			//open a new socket, send on the new one
			removeFromPollSet(socketNum);
			close(socketNum);
			socketNum = setupUdpClientToServer(server, params.remote_machine, current_server_port);
			//!do i need to re-add it, wont it just do this at the top?
			addToPollSet(socketNum);

			sends_attempted++;
			continue;
		}
		// now the case that the timer has NOT expired
		// it returned a valid socket to read so its going to recieve data
		// after it receives it and the flag is the filename ack flag it will go to get data state
		else {
			int recieved = recvfrom(socketNum, recv_buffer, ACK_BUFF_SIZE, 0, (struct sockaddr *) server, &serverAddrLen);
			
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

			//if file ack receivied from server, its coming from a child 
			//need to open a new socket to talk to the child
        	if (response_flag == 9) {

            	return FILE_VALID;
        	} 
			// if the response is a "talk to here now" packet
			// stop talking to the main server and redirect all communication to this new port
			// reset the poll
			// reset the counter
			else if (response_flag == 33) { 

				//! update the global variable
				current_server_port = ntohs(server->sin6_port);
				printf("WHAT IS THE CURRENT SERVER PORT: %d\n", current_server_port);

				printf("got the 'talk to here now' packet. redirecting to child server\n");
				printf("New child server port received: %d\n", current_server_port);

				removeFromPollSet(socketNum);
            	close(socketNum);
				socketNum = setupUdpClientToServer(server, params.remote_machine, current_server_port);
				printf("now sending with this socketNum: %d\n", socketNum);
				addToPollSet(socketNum);
				// reset the timer and the sends, resend to the new socket
				sends_attempted = 0;
				timeout = 1000; 
            	continue; 
        	} 
			else {
				// if the response flag is 32, then I know there is an error, and I move to DONE
            	removeFromPollSet(socketNum);
            	return DONE;
        	}
    	}
	}
	//the case where the sends_attempted are greater than 9
	removeFromPollSet(socketNum);
	return DONE;
}

int doFileValidState(RcopyParams params){
	curr_file_open = fopen(params.dest_filename, "wb");

	if (curr_file_open == NULL){
		perror("Couldn't open file\n");
		return DONE;
	}

	//!do i need to re-add it, wont it just do this at the top?
	//addToPollSet(socketNum);

	return GET_DATA;
}

int doGetDataState(){
	//- do nothing here for now, will implement this when we get into the sliding window? 
    //- need to do sliding window for client and server?
	printf("reached get data state\n");
	return DONE;
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


// void talkToServer(int socketNum, struct sockaddr_in6 * server)
// {
// 	int serverAddrLen = sizeof(struct sockaddr_in6);
// 	char * ipString = NULL;
// 	int dataLen = 0; 
// 	char buffer[MAXBUF+1];
	
// 	buffer[0] = '\0';
// 	while (buffer[0] != '.')
// 	{
// 		dataLen = readFromStdin(buffer);

// 		printf("Sending: %s with len: %d\n", buffer,dataLen);
	
// 		safeSendto(socketNum, buffer, dataLen, 0, (struct sockaddr *) server, serverAddrLen);
		
// 		safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *) server, &serverAddrLen);
		
// 		// print out bytes received
// 		ipString = ipAddressToString(server);
// 		printf("Server with ip: %s and port %d said it received %s\n", ipString, ntohs(server->sin6_port), buffer);
	      
// 	}
// }

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