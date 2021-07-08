#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


//length of messages to server
#define MAX_LENGTH 100

void error(const char *msg) {perror(msg); exit(1);}

int check_length(char*, char*);
void encrypt(char*,char*,char*,char*, int,int);
void decrypt(char*,char*, int);
int decode(char, char);
int encode(char, char);

int main(int argc, char *argv[])
{
	int socketFD, portNumber, charsWritten, charsRead, key_len;
	struct sockaddr_in serverAddress;
	struct hostent* serverHostInfo;
	char buffer[MAX_LENGTH];
	char message[MAX_LENGTH];
	char *temp;

	//checks if too few arguments

	if(argc < 3) { fprintf(stderr, "USAGE: %s hostname port\n", argv[0]); exit(0);}
	
	//checks if post type and sets key_len to length of key file
	if(strcmp(argv[1], "post") == 0) key_len = check_length(argv[3], argv[4]);
	//exits if no key value
	if(key_len == -1) return 0;	

	memset((char*)&serverAddress, '\0', sizeof(serverAddress));
	if(strcmp(argv[1], "post") == 0) portNumber = atoi(argv[5]);
	else portNumber = atoi(argv[4]);
//	portNumber = atoi(argv[2]);
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(portNumber);
	serverHostInfo = gethostbyname("localhost");
	if(serverHostInfo == NULL) {fprintf(stderr, "CLIENT: ERROR, no such host\n"); exit(0);}
	memcpy((char*)&serverAddress.sin_addr.s_addr, (char*)serverHostInfo->h_addr, serverHostInfo->h_length);
	
	socketFD = socket(AF_INET, SOCK_STREAM, 0);
	if(socketFD < 0) error("CLIENT: ERROR opening socket");
	
	if(connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) error("CLIENT: ERROR connecting");

	

//	printf("CLIENT: Enter text to send to the server, and then hit enter: ");
	memset(buffer, '\n', sizeof(buffer));

	//routes to encrypt if type is post
	if(strcmp(argv[1], "post") == 0)
	{
		encrypt(argv[3], argv[4], argv[1], argv[2], socketFD, key_len);
	}
	//sends message to server for get if not post
	else
	{
		strcpy(message, argv[1]);
		strcat(message, " ");
		strcat(message, argv[2]);
		message[strcspn(message, "\n")] = '\0';
		charsWritten = send(socketFD, message , strlen(message), 0);
	}

//	fgets(buffer, sizeof(buffer) - 1, stdin);
//	message[strcspn(message, "\n")] = '\0';
//	charsWritten = send(socketFD, message , strlen(message), 0);
	
//	if(charsWritten < 0) error("CLIENT: ERROR writing to socket");
//	if(charsWritten < strlen(message)) printf("CLIENT: WARNING: Not all data written to socket!\n");
	
	memset(buffer, '\0', sizeof(buffer)); 
	if(strcmp(argv[1], "get") == 0)
	{

//calls decrypt function if type get
		decrypt(buffer, argv[3], socketFD);
	}
	close(socketFD);

	//outputs last characters for get request
	buffer[0] = '\n';
	buffer[1] = '\0';
	fprintf(stdout,"%s", buffer);	
	return 0;
}

//decrypts the servers response

void decrypt(char *message, char *filename, int connection)
{
	FILE *fp;
	int count, charsRead, con = 1;
	char buffer[MAX_LENGTH+2];
	char output[MAX_LENGTH];
	char c;	
	
	//opens key file to decrypt server message
	fp = fopen(filename, "r");
	if(fp == NULL) error("ERROR: client opening key file for decrypting");

	//whlie not end of server message
	while(con == 1)
	{
		//gets message from server
		charsRead = recv(connection, buffer, sizeof(buffer) , 0);
		if(charsRead < 0) error("CLIENT; error reading from socket");

		//if servers returns 38, no file for the user was found and exits
		if(buffer[0] == 38) {fprintf(stderr,"File not found\n");exit(1);}

		//gets each character from message and decodes
		for(count = 0; count < MAX_LENGTH; count++)
		{	
			c = fgetc(fp);
			//if new line (last character in file) then break
			if(c == 10){break;}
			//if not a valid character then break
			if(buffer[count] != 32 && (buffer[count] < 65 || buffer[count] > 90)) break;
			//call decode on the message character and key character
			c = decode(buffer[count], c);
			//write to standard output
			fprintf(stdout,"%c",c);
		}

		//if last character is 39 then message will continue
		if(buffer[count] == 39) 
		{
			//acknowledge to server that client is ready for next message
			charsRead = send(connection, buffer, sizeof(buffer) -1, 0);
			memset(buffer, '\0', sizeof(buffer));
		}
		//if not continuing then set con to 0 to break loop
		else con = 0;
	}
	//cloes key file
	fclose(fp);
}

//decode takes message character and key character
//returns number representing ascii character value
int decode(char x, char y)
{
	//converts characters to number between 0 and 27
	int i, j, sum = 0;	
	if(x == 32) i = 0;
	else i = x - 64;
	if(y == 32) j = 0;
	else j = y - 64;
	
	//subtracts key character from message 
	//adds 27 to character if it turns negativd
	sum = i - j;
	if(sum < 0)
	{
		sum = sum + 27;
	}	
	//if 0, sets character to space value
	if(sum == 0) sum = 32;
	//else converts to capital character
	else sum = 64 + sum;

	return sum;
}

//takes data message file to encrypt, key file, post type, user name, connection number and key character length
//handles encrypting the message file and sending to server for storage
void encrypt(char *message, char *filename, char *type, char *username, int connection, int key_len)
{
	FILE *key_file, *message_file;
	int count = 0, charsWritten, charsRec, con = 1, length = 0, end_of_mess = 0;
	char key[MAX_LENGTH+1];
	char plaintext[MAX_LENGTH+1] = {'\0'};
	char output[MAX_LENGTH+3] = {'\0'}; 
	char c, place_holder = 0, msg_len;
	memset(output,'\0', sizeof(output));
		
	//opens key and message files for reading
	key_file = fopen(filename,"r");
	message_file = fopen(message,"r");
	if(key_file == NULL) error("ERROR: client opening key file");
	if(message_file == NULL) error("ERROR: client opening message file");
	
	//sets up first message to server. Tells it it's a post request for user 'xyz' and that messages will follow
	//39 is interpretted as a continuation
	strcpy(output, type);
	output[strlen(output)] = ' ';
	strcat(output, username);
	output[strlen(output)+1] = 39;	
	output[strlen(output)+2] = '\0';

	//sends to server
	charsWritten = send(connection, output, strlen(output), 0);
	if(charsWritten < 0) error("CLIENT: ERROR writing to socket");
	if(charsWritten < strlen(output)) printf("CLIENT: WARNING: Not all data written to socket\n");

	//receives acknowledgement from server
	charsRec = recv(connection, output, strlen(output), 0);
	
	//while not end of the message
	while(con == 1)
	{
		//reset values of strings
		memset(key, '\0', sizeof(key));
		memset(output, '\0', sizeof(output));
		memset(plaintext, '\0', sizeof(plaintext));

		//gets first string of message from message file
		for(count = 0; count < MAX_LENGTH; count++)
		{
			c = fgetc(message_file);
//			printf("c:%c\t%d\n", c,c);
			//breaks if end of file and sets continuation to false
			if(!feof || c == 10 || c == -1){con = 0;  break;}
			//if valid  input, stores in message string
			else if(c == 32 || c < 91 && c > 64) plaintext[count] = c;
			//if invalid inputer outputs error
			else {error("ERROR: client invalid plaintext value");}
		}
		if(con == 0)
		message[MAX_LENGTH+1] = '\0';			
		//sets msg_len to count value. used to get correct number of key characters
		msg_len = count;
//		printf("plaintext:%s\ncon:%d\n",plaintext,con);
	
		//gets string of characters from key file
		for(count = 0; count < msg_len; count++)
		{
			c = fgetc(key_file);
			//if valid input, stores c in key string
			if(c == 32 || c < 91 && c > 64) key[count] = c;
			else if(c == 10) {break;}
			//errors out if invalid key file character
			else {error("ERROR: client invalid key value");}
		}

		//this loop encrypts the message string with the key string and stores in output
		for(count = 0; count < msg_len; count++)
		{
			//passes each character in strings to encode and stores value in outptu file
			c = encode(plaintext[count], key[count]);
			if(c == 0) output[count] = 32;//if encode returns 0 stores space character
			else output[count] = c + 64;//if not 0 then stores as capital character
	
		}
		//if contintuing, ends with 39 for server to know to keep listening
		if(con == 1) output[MAX_LENGTH] = 39;
		else output[MAX_LENGTH+1] = '\0';

//		printf("output:%s\tcon:%d\n",output,con);
		//sends output message to server
		charsWritten = send(connection, output, strlen(output), 0);
		if(charsWritten < 0) error("CLIENT: ERROR writing to socket");
		if(charsWritten < strlen(output)) printf("Client:warning not all data written");		
		if(key_len == length) con = 0;
		//listens for response if message continuing
		if(con == 1){	charsRec = recv(connection, output, sizeof(output), 0);}
	}

	//close key and message files
	fclose(key_file);
	fclose(message_file);
	
}


//takes message and key characters and reutnrs 0 through 26
int encode(char x, char y)
{
	//converst message and key to 0 through 27
	int i, j, sum = 0;
	if(x == 32) i = 0;
	else i = x - 64;
	if(y == 32) j = 0;
	else j = y - 64;

	//adds them together, modules them and returns int value
	sum = i + j;
	sum = sum % 27;
	return sum;	 		
}

//check length takes message and key files names and compares their lengths
//returns error and -1 value if key file is too short
int check_length(char *message_file, char*key_file)
{
	FILE *mess, *key;
	int key_count, mess_count;
	char c;
	key_count = mess_count = 0;

	//open files
	mess = fopen(message_file, "r");
	key = fopen(key_file, "r");

	if(mess == NULL) printf("client: erorr opening message file\n");
	if(key == NULL) printf("client: eorror opening key file\n");

	//count characters of key
	while(1)
	{
		c = fgetc(key);	
		if(feof(key)) break;
		key_count++;		
	}
	//count characters of message
	while(1)
	{
		c = fgetc(mess);
		if(feof(mess)) break;
		mess_count++;
	}	
	//compares lengths of key and message. Errors out if key is too short
	if(mess_count > key_count) {key_count = -1; error("ERROR: client key is too short");}
	fclose(key);
	fclose(mess);
	return key_count;
}

