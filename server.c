/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include "server.h"

int main ( int argc, char *argv[]  )
{ 
	int socketNum = 0;				

	ServerParams serverParam = checkArgs(argc, argv);
		
	socketNum = udpServerSetup(serverParam.port_number);

	processClient(socketNum);

	close(socketNum);
	
	return 0;
}

void processClient(int socketNum)
{
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
		// can proceed to extract the correct info and send back the flag 9 correct ack packet
		memcpy(filename, buffer + 7, MAX_FILENAME_LEN);
		memcpy(buffer_size_buff, buffer + 107, 2);
		memcpy(window_size_buff, buffer + 109, 4);

		// make the packet for the correct ack
		uint8_t temp_buff = makeVALIDFilenameACKBeforeChecksum();
		uint16_t calculated_checksum = calculateFilenameChecksum(temp_buff);
		uint8_t ack_buff_send = makeFilenameACKAfterChecksum(temp_buff, calculated_checksum);

		// send it back to the client
		// how do i implement the state machine here? 

		//!DEBUG: add print statements to check that it was read out properly
		printf("checksum calculated right: %d\n", result);
		
	}
	else{
		// send back the flag 32 error ack packet
		printf("checksum not right\n");
		// make the packet for the correct ack
		uint8_t temp_buff = makeERRORFilenameACKBeforeChecksum();
		uint16_t calculated_checksum = calculateFilenameChecksum(temp_buff);
		uint8_t ack_buff_send = makeFilenameACKAfterChecksum(temp_buff, calculated_checksum);

		// send it back to the client
		// how do i implement the state machine here? 
	}


	//! the code that was in this function originally
	// buffer[0] = '\0';
	// while (buffer[0] != '.')
	// {
	// 	dataLen = safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *) &client, &clientAddrLen);
	
	// 	printf("Received message from client with ");
	// 	printIPInfo(&client);
	// 	printf(" Len: %d \'%s\'\n", dataLen, buffer);

	// 	// just for fun send back to client number of bytes received
	// 	sprintf(buffer, "bytes: %d", dataLen);
	// 	safeSendto(socketNum, buffer, strlen(buffer)+1, 0, (struct sockaddr *) & client, clientAddrLen);

	// }
}

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