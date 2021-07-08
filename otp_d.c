#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>

//MAX_LENGTH defines maximum length of messages to send
#define MAX_LENGTH 100


//counts number of processes that are active
int process_count = 0;


//declares helper functions for readability
void error(const char *msg){perror(msg);exit(1);}
void handle_connection(int);
void user_post(char*, int);
void user_get(char*,int);

//defines child handler function
//function decrements process count when a child process finishes
void child_handler(int signo)
{
	int x;
	pid_t pid;
	pid = waitpid(-1, &x, WNOHANG | WUNTRACED);
	
	process_count = process_count - 1;
}


int main(int argc, char *argv[])
{
	struct sigaction child = {0}; 
	pid_t childPid;
	int listenSocketFD, establishedConnectionFD, portNumber, charsRead;
	socklen_t sizeOfClientInfo;
	char buffer[MAX_LENGTH];
	struct sockaddr_in serverAddress, clientAddress;

	child.sa_handler = child_handler;

	//sets SIGCHLD interrupt function
	//decrements process count
	signal(SIGCHLD, child_handler);

	//exits if too few arguments
	if (argc < 2) { fprintf(stderr, "USAGE: %s port\n", argv[0]); exit(1);} 

	//set up the addres struct for this process
	memset((char *)&serverAddress, '\0', sizeof(serverAddress));
	portNumber = atoi(argv[1]);
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(portNumber);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	
	//setup the stocket
	listenSocketFD = socket(AF_INET, SOCK_STREAM, 0);
	if(listenSocketFD < 0) error("ERROR opening socket");
	
	//enable sockt to begin listening
	if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) error("ERROR on binding");
	listen(listenSocketFD, 5);

	sizeOfClientInfo = sizeof(clientAddress);

	//keeps otp_d parent listenting forever
	while(1)
	{
		//waits for process to finish if there are more than 5 funning
		while(1)
		{
			//if fewer than 5 process running go establish next connection
			if(process_count < 5)
			{
				break;
			}
		}
		//establishes connection
		establishedConnectionFD = accept(listenSocketFD, (struct sockaddr*)&clientAddress, &sizeOfClientInfo);
		if(establishedConnectionFD < 0) error("ERROR on accept");
		
		//creates child porcess to handle the created connection
		if((childPid = fork()) == 0)
		{
			//sleeps 2 per instructions
			sleep(2);
			//stops child from listening on socket
			close(listenSocketFD);
			//handles established connection
			handle_connection(establishedConnectionFD);	
		}
		else
		{
			//parent process increments process count and continues to listen
			process_count++;
		}
	} 
	
/*
	memset(buffer, '\0', 256);
	charsRead = recv(establishedConnectionFD, buffer, 255, 0);
	if(charsRead < 0) error("ERROR reading from socket");
	printf("SERVER: I received this from the client: \"%s\"\n", buffer);

	handle_connection(establishedConnectionFD);
	
	charsRead = send(establishedConnectionFD, "I am the server, and I got your message", 39, 0);
	if (charsRead < 0) error("ERROR writing to socket");
*/
	close(listenSocketFD);
	return 0;

}

//handle connection takes established connection and passes to other functions
//it looks at the type of request and passes along the message - the type
void handle_connection(int connection)
{
	int charsRead, x, ack;
	char buffer[MAX_LENGTH];
	char new_buffer[MAX_LENGTH];	


	
	memset(buffer, '\0', MAX_LENGTH);
	charsRead = recv(connection, buffer, MAX_LENGTH, 0);
	if(charsRead < 0) error("ERROR reading from socket");
//	printf("SERVER: I received this from the client: \"%s\"\n", buffer);

//if buffer is a post request passes remaining message to user_post function
	if(buffer[0] == 'p')
	{
		for(x = 0; x < strlen(buffer); x++)
		{
			new_buffer[x] = buffer[x+5];
		}
		user_post(new_buffer, connection);
	}
//if it's not a post function, passes remaining message to user_get function
	else
	{
//		printf("running get\n");

		for(x = 0; x < strlen(buffer); x++)
		{
			new_buffer[x] = buffer[x+4];
		}
		user_get(new_buffer, connection);
	}
	close(connection);
	exit(0);
}

//user post function takes input from handle_connection 
//handles the reminaing data communication and storing file
void user_post(char * buffer, int connection)
{
	FILE *fp;
	char filename[256];
	char temp[256];
	char message[MAX_LENGTH+2];
	char path[256];
	char last;
	int x = 0, charsRead;


//	printf("%s\n", buffer);
	
	//gets the user name from the message
	//used to create file name
	while(buffer[x] != ' ')
	{
		if(buffer[x] == 39) break;
		filename[x] = buffer[x];
		x++;
	}

	x++;
	
	//gets the pid and adds it to the user name to create a unique filename
	sprintf(temp, "%d", getpid());
	strcat(filename, temp);
	fp = fopen(filename, "w");
	
	//opens file for writing and errors out if there are issues opening
	if(fp == NULL) error("SERVER: error opening post file for writing");
	//sends response to otp to confirm it is ready for additional input
	charsRead = send(connection, temp, strlen(temp), 0);
	if(charsRead < 0) error("ERROR: server post writing to socket");

//	printf("Sent\n");

	//setting up values
	//39 is value programs send when they have additional messages to send
	last = 39;
	x = 0;
	while(last == 39)
	{
		x = 0;
		//waits to receive the encrypted message
		charsRead = recv(connection, message, MAX_LENGTH + 1, 0);
		if(charsRead < 0) error("ERROR read from socket");

		//writes each character of message into open file while not the end of the message
		while(x < strlen(message))
		{
			if(message[x] != 39) fputc(message[x], fp);
			x++;
		}
		//if 39 is last value then continue looping as more message to come. else break
		last = message[x-1];
	//printf("last:%d\n",last);

	//acknowledge that message was received and to continue sending
	if(last == 39)
	{
		charsRead = send(connection, temp, strlen(temp), 0);
		if(charsRead < 0) error("ERROR: server post writing to socket");
	}
	memset(message, '\0', sizeof(message));
	}

	//add new line to the end of the file and close
	fputc('\n', fp);
	fclose(fp);

	//get path of the file and print it out
	getcwd(path, 256);
	strcat(path, "/");
	strcat(path,filename);

	printf("%s\n",path);

}

//receives input from handle_connection
//reads from file and sends to client

void user_get(char *buffer, int connection)
{
	DIR *dir;
	FILE *fp;
	struct dirent *sd;
	time_t high = 0;
	char filename[256];
	char output[MAX_LENGTH+2] = {'\0'};
	char user[MAX_LENGTH];
	char c;
	struct stat buf;
	int x = 0, y = 0, charsRead;	

	dir = opendir(".");
	
	//read user name from message to find file
	while(buffer[x] != ' ')
	{
		user[x] = buffer[x];
		x++;
	}
	x++;

//loops through directory and checks if user has a file
//	printf("user:%s\n",user);
	if(dir == NULL) {printf("error\n"); exit(1);}
	
	while( (sd=readdir(dir)) != NULL)
	{
		//if file exists get information on it
		if(strstr(sd->d_name, user) != NULL)
		{
			stat(sd->d_name, &buf);
			//if file is not the first file found or is the oldest then update as new oldest file
			if(high == 0 || buf.st_mtime < high)
			{
				high = buf.st_mtime;
				strcpy(filename, sd->d_name);
			}
		}
	}		
	//after looping through directory it should have the oldest user file
	//and opens that one
	fp = fopen(filename, "r");
	
	//if no file is found. sends 38 to signal no file found and exits
	if(fp == NULL)
	{
	//	printf("Sending\n");
		output[0] = 38;
		output[1] = '\0';
		
		charsRead = send(connection, output, strlen(output), 0);
		close(connection);
		exit(0);
	}

	
	x = 1;
	while(x ==1)
	{
		//loops through file while char is not a mex message length and stores in output string
		for(y = 0; y < MAX_LENGTH; y++)
		{
			c = fgetc(fp);
			if(feof(fp)) {x =0; break;}
			output[y] = c;
		}
		//if not end of the file, add 39 to the end to signal message will continue
		//else set to 0 to end string
		if(x == 1) {output[y] = 39;output[y+1] = '\0';}
		else output[y] = '\0';	
//	printf("%s\n", outpu
		//printf("server output:%s\nx:%d\n", output,x);
		charsRead = send(connection, output, strlen(output), 0);
		if(charsRead < 0) error("ERROR: server writing to socket");

		//if message contintues wait for client to acknowledge receipt
		if(x == 1) charsRead = recv(connection, output, strlen(output), 0);
		else break;
	}
	
	//close and remove file after sending message
	fclose(fp);
	remove(filename);	

	
	close(connection);
}






