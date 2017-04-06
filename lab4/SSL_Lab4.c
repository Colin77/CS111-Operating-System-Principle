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

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

//server cmd flag
int on_flag = 1;
int measure_interval = 3;
int STOP_FLAG = 0;
int C_FLAG = 0;

//client socket variable
int client_socket_fd, port_num;
struct sockaddr_in serv_addr;
struct hostent *server;
char received_buffer[256];
char send_buffer[256];
SSL* ssl;

//log file
FILE* log_file_fd;

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

	//open SSL parameter
	const SSL_METHOD* method;
	SSL_CTX* ctx;
	X509* cert = NULL;
	X509_NAME* certname = NULL;


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

	//innitialize openssl
	OpenSSL_add_all_algorithms();
	ERR_load_BIO_strings();
	ERR_load_crypto_strings();
	SSL_load_error_strings();

	//initialize SSL lib and reg algo
	if(SSL_library_init() < 0){
		perror("Could not initilize the openSSL lib");
		exit(EXIT_FAILURE);
	}

	//announce + creating new SSL context
	method = SSLv23_client_method();
	ctx = SSL_CTX_new(method);
	if(ctx == NULL){
		perror("Unable to create a new SSL context struct");
		exit(EXIT_FAILURE);
	}

	//negotiation
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);

	//create new SSL connection state object
	ssl = SSL_new(ctx);

	port_num = atoi(argv[2]);
	//Get Server info
	server = gethostbyname(argv[1]);
	if(server==NULL){
		perror("Error on translating address");
		exit(EXIT_FAILURE);
	}

	//client fd
	client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(client_socket_fd < 0){
		perror("Error creating client socket");
		exit(EXIT_FAILURE);
	}

	//set server info
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
	serv_addr.sin_port = htons(port_num);

	//establish connection to server
	if(connect(client_socket_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr))<0){
		perror("Unable to create connection to server");
		exit(EXIT_FAILURE);
	}
	printf("TCP connection with remote server established\n");

	//Attach SSL session to the socket descriptor
	SSL_set_fd(ssl, client_socket_fd);

	//Try SSL_connect here, return 1 for success
	if(SSL_connect(ssl) != 1){
		perror("SSL connection fail");
		exit(EXIT_FAILURE);
	}
	printf("SSL connection established\n");
	
	//get remote certificate into X509
	cert = SSL_get_peer_certificate(ssl);
	if(cert == NULL){
		perror("Cant get certificate");
		exit(EXIT_FAILURE);
	}
	printf("Obtained SSL certification\n");

	//extract cert name
	certname = X509_NAME_new();
	certname = X509_get_subject_name(cert);

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

		
	//housekeeping
	pthread_join(receive_thread,NULL);
	pthread_join(send_thread,NULL);
	SSL_free(ssl);
	fclose(log_file_fd);
	X509_free(cert);
	SSL_CTX_free(ctx);
	mraa_aio_close(0);

	return 0;
}


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
			n = SSL_write(ssl, send_buffer, sizeof(send_buffer));
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
		n = SSL_read(ssl, received_buffer, sizeof(received_buffer));
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