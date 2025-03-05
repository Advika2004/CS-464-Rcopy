/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include "server.h"

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
	int socketNum = 0;				

	ServerParams serverParam = checkArgs(argc, argv);
		
	socketNum = udpServerSetup(serverParam.port_number);

	// need to poll on the incoming socket for the filename connection
	setupPollSet();
	addToPollSet(socketNum);
	// need to poll forever ? so that i can manage all incoming clients?
	int poll = pollCall(-1);

	//need to call this here bcasue need to call this before calling sendErr?
	sendErr_init(serverParam.error_rate, DROP_ON, FLIP_OFF, DEBUG_ON, RSEED_OFF);

	int dataLen = 0; 
	char buffer[114];	  
	struct sockaddr_in6 client;		
	int clientAddrLen = sizeof(client);	

	// will extract the values from the client and store it in here
	char filename[MAX_FILENAME_LEN];
	uint8_t buffer_size_buff[MAX_BUFFER_LEN];
	uint8_t window_size_buff[MAX_WINDOW_LEN];

	//take in the filename packet from the client
    int recv_len = recvfrom(socketNum, buffer, 114, 0, (struct sockaddr*)&client, &clientAddrLen);
    if (recv_len < 0) {
        perror("recvfrom failed");
        return;
    }

	//calculate the checksum and see if its valid 
	uint16_t result = calculateFilenameChecksum(buffer);
	if (result == 0){
		// can proceed to extract the correct info
		memcpy(filename, buffer + 7, MAX_FILENAME_LEN);
		memcpy(buffer_size_buff, buffer + 107, 2);
		uint16_t host_buffer_size = ntohs(buffer_size_buff);
		memcpy(window_size_buff, buffer + 109, 4);
		uint16_t host_window_size = ntohl(window_size_buff);

		//!DEBUG: add print statements to check that it was read out properly
		printf("checksum calculated right: %d\n", result);
		startFSM(filename, host_buffer_size, host_window_size, &client, clientAddrLen);
	}

	else{
		continue;
		// keep polling? // what do I put here> 
	}

	//! do I need this here? 
	close(socketNum);
	return 0;
}

void startFSM(char* filename, uint16_t buffer_size, uint32_t window_size, (struct sockaddr*)&client, int clientAddrLen){
	int state;

	// need to make a child now
	pid_t server_child = fork();
	if (server_child < 0) {
		perror("fork failed");
		//! what to do when the fork fails? 
		continue;
	}

	// now within the child start the fsm
	// close the main server socket since that is not used anymore in here
	if (server_child == 0) { 
		close(socketNum); 
			
		// Open a new socket for communication with this client
		//os will give it a random one
		int child_server_socket = udpServerSetup(0);

		//send the "talk to here now" packet to the client
		uint8_t temp_buff = makeTalkHereNowBeforeChecksum();
		uint16_t calculated_checksum = calculateFilenameChecksum(temp_buff);
		uint8_t talk_buff_send = makeTalkHereNowaAfterChecksum(temp_buff, calculated_checksum);

		uint16_t result = calculateFilenameChecksum(talk_buff_send);
		if (result == 0){
			printf("checksum calculated right: %d\n", result);
		}
		else{
			printf("checksum not right\n");
			exit(1);
		}

		// Send the "talk to here" packet
		int bytes_sent = sendtoErr(child_server_socket, talk_buff_send, sizeof(talk_buff_send), 0,
                           (struct sockaddr*)&client, clientAddrLen);
		if (bytes_sent < 0) {
    		perror("sendtoErr failed\n");
    		return DONE;
		}

		state = GET_FILENAME
	}

//while the state is not the done state
    while (state != DONE) {
		//check the state
        switch (state) {
            case GET_FILENAME:
                state = doGetFilenameState(filename);
                break;
            case DONE:
                printf("h");
                state = doDoneState();
                break;
            case SEND_DATA:
                printf("h");
                state = doSendDataState();
                break;
            case WAIT_ON_ACK:
                printf("h");
				state = doWaitOnAckState();
                break;
			case WAIT_ON_EOF_ACK:
                printf("h");
				state = doWaitOnEOFAckState();
                break;
        }
    }
}

int doGetFilenameState(char* filename) {

	curr_file_open = fopen(filename, "rb");

	if (curr_file_open == NULL){
		perror("Couldn't open file\n");

		// make the packet for the correct ack
		uint8_t temp_buff = makeVALIDFilenameACKBeforeChecksum();
		uint16_t calculated_checksum = calculateFilenameChecksum(temp_buff);
		uint8_t ack_buff_send = makeFilenameACKAfterChecksum(temp_buff, calculated_checksum);

		uint16_t result = calculateFilenameChecksum(buffer_to_send);
		if (result == 0){
			printf("checksum calculated right: %d\n", result);
		}
		else{
			printf("checksum not right\n");
			exit(1);
		}

		//!send the packet

		// must send error ack packet 

		return DONE;
	}

	// else send the right ack packet 
	// send back the flag 32 error ack packet
	printf("checksum not right\n");
	// make the packet for the correct ack
	uint8_t temp_buff = makeERRORFilenameACKBeforeChecksum();
	uint16_t calculated_checksum = calculateFilenameChecksum(temp_buff);
	uint8_t ack_buff_send = makeFilenameACKAfterChecksum(temp_buff, calculated_checksum);

	uint16_t result = calculateFilenameChecksum(buffer_to_send);
	if (result == 0){
		printf("checksum calculated right: %d\n", result);
	}
	else{
		printf("checksum not right\n");
		exit(1);
	}

	//! send the packet

	return SEND_DATA_STATE

}
int doDoneState() {
	return 0;
}
int doSendDataState() {
	return 0;
}
int doWaitOnAckState() {
	return 0;
}
int doWaitOnEOFAckState() {
	return 0;
}


// void processClient(int socketNum)
// {
// 	//! the code that was in this function originally
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

uint16_t calculateFilenameChecksum(uint8_t* buffer){
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

	//flag for "send here now" is 32
	uint8_t flag = 33;
    memcpy(buffer + 6, &flag, 1);

    return buffer;
}

uint8_t* makeTalkHereNowAfterChecksum(uint8_t* buffer, uint16_t calculated_checksum){

	//put in the calculated checksum
    memcpy(buffer + 4, &calculated_checksum, 2);

    return buffer;
}