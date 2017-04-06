#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <mraa/aio.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>

//server cmd flag
int on_flag = 1;
int measure_interval = 3;
int STOP_FLAG = 0;
int C_FLAG = 0;

//log file
FILE* log_file_fd;

//client socket variable
int client_socket_fd, port_num;
struct sockaddr_in serv_addr;
struct hostent *server;
char received_buffer[256]; //buffer to store the msg received
char send_buffer[256]; //buffer to strore message sent to server

//timestamp varialble
time_t raw_time;
struct tm* time_info;
char time_buffer[80];

//thermistor realted variable
mraa_aio_context temp_measure;
double value;
float reading;
float real_temp;
const int B = 4275;

void *send_data();
void *receive_data();
void * cmd_handler(char *buffer);

int main(int argc, char*argv[])
{

	//thread
	int rc1, rc2;
	pthread_t send_thread, receive_thread;

	temp_measure = mraa_aio_init(0);

	if(argc < 3){
		printf("Usage: IP address name + Port number\n");
		exit(EXIT_FAILURE);
	}

	//open a log file to store all the data and command from server
	log_file_fd =fopen("Activity_Log.txt", "a");
	if(log_file_fd==NULL){
		fprintf(stderr, "Cannot create log file!\n");
	}
	//Go to the head of the file to orverwrite previous data may already exist
	fseek(log_file_fd, 0, SEEK_SET);

////////////////////////////////////////////////////////////////////////////////////////////
//Establishing connection with server via TCP/IP///////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////

	//get the port number
	port_num = atoi(argv[2]);

	//get the server info
	server = gethostbyname(argv[1]);
	if(server == NULL){
		perror("Error on translating address");
		exit(EXIT_FAILURE);
	}

	//Setup the client socket
	client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(client_socket_fd < 0){
		perror("Error creating client socket");
		exit(EXIT_FAILURE);
	}
	//setup the server address and write server info to it(serv_addr) from "server" struct
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
	serv_addr.sin_port = htons(port_num);
	//connect to server
	if(connect(client_socket_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))<0){
		perror("Cannot connect to server");
		exit(EXIT_FAILURE);
	}

	//connected to server, read server's command into a buffer
	printf("Connected to server!\n");
	write(client_socket_fd, "704205565", 10);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
/////////////////////////////////////////creating sending/receiving data routine into threads////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
	
	//creating thread to send the data
	rc1 = pthread_create(&send_thread, NULL, send_data, NULL);
	if(rc1){
		printf("Error on creating thread1\n");
		exit(EXIT_FAILURE);
	}

	//creating thread to send the data
	rc2 = pthread_create(&receive_thread, NULL, receive_data, NULL);
	if(rc2){
		printf("Error on creating thread2\n");
		exit(EXIT_FAILURE);
	}

		

	pthread_join(receive_thread,NULL);
	pthread_join(send_thread,NULL);
	fclose(log_file_fd);
	mraa_aio_close(0);

	return 0;

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////Aux functions beyond this point//////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//thread fucntion
void* send_data()
{
	int n;
	while(on_flag){
		if(!STOP_FLAG){
			value = mraa_aio_read(temp_measure);
			//convert raw reading to temperature
			reading = 1023.0/((float)value)-1.0;
			reading = 100000.0*reading;
			real_temp = 1.0/(logf(reading/100000.0)/B+1/298.15)-273.15;
			real_temp = real_temp*1.8+32;
			//convert Centigrade to C if prompted
			if(C_FLAG){
				real_temp = (real_temp-32)/1.8;
			}
			//get timestamp
			time(&raw_time);
			time_info = localtime(&raw_time);
			strftime(time_buffer, 80, "%H:%M:%S", time_info);
			//Formating the whole data
			sprintf(send_buffer, "704205565 TEMP = %0.1f", real_temp);
			//Loggong the data
			fprintf(log_file_fd, "%s %0.1f\n", time_buffer, real_temp);
			fflush(log_file_fd);
			//Sending data to server
			n = write(client_socket_fd, send_buffer, sizeof(send_buffer));
			printf("Finished writing data: %s\n",send_buffer);
			if(n<0){
				perror("Error on writing");
				exit(EXIT_FAILURE);
			}
			sleep(measure_interval);
		}	
	}
	pthread_exit(NULL);
}

void* receive_data()
{
	printf("Try receive data..\n");
	int n;
	while(on_flag){
		memset(received_buffer, 0, 256);
		n = read(client_socket_fd, received_buffer, sizeof(received_buffer));
		if(n<0){
			perror("Error on reading from server");
			exit(EXIT_FAILURE);
		}
		printf("Message received: %s\n", received_buffer);
		//call command handling function
		cmd_handler(received_buffer);

	}
	pthread_exit(NULL);
}

void * cmd_handler(char *buffer)
{
	int temp_p = 3;
	if(strcmp(buffer, "OFF") == 0){
		printf("Turning off devices now...\n");
		//write the cmd into log
		fprintf(log_file_fd, "%s\n", received_buffer);
		fflush(log_file_fd);
		on_flag = 0;
		return;

	}

	else if(strcmp(buffer, "STOP")==0){
		STOP_FLAG = 1;
		//write the cmd into log
		fprintf(log_file_fd, "%s\n", received_buffer);
		fflush(log_file_fd);
		return;
	}

	else if(strcmp(buffer, "START")==0){
		STOP_FLAG = 0;
		//write the cmd into log
		fprintf(log_file_fd, "%s\n", received_buffer);
		fflush(log_file_fd);
		return;
	}

	else if(buffer[0] == 'P'){
		sscanf(buffer, "PERIOD=%d", &temp_p);
		if(temp_p > 1 && temp_p < 3600){
			measure_interval = temp_p;
			//write the cmd into log
			fprintf(log_file_fd, "%s\n", received_buffer);
			fflush(log_file_fd);
			return;
		}
			
	}

	else{
		char c;
		sscanf(buffer, "SCALE=%c", &c);
		if(c == 'F'){
			C_FLAG = 0;
			//write the cmd into log
			fprintf(log_file_fd, "%s\n", received_buffer);
			fflush(log_file_fd);
			return;
		}
		else if(c == 'C'){
			C_FLAG = 1;
			//write the cmd into log
			fprintf(log_file_fd, "%s\n", received_buffer);
			fflush(log_file_fd);
			return;
		}
	}

	//Only invalid command would come to this point
	//Append Invalid mark and wirte to log file	
	strcat(received_buffer, " I");
	fprintf(log_file_fd, "%s\n", received_buffer);
	fflush(log_file_fd);	

}
