#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<signal.h>
#include<getopt.h>
#include<string.h>

void sig_handler(int);
#define BUF_SIZE 8192

int main(int argc, char** argv)
{
	int ch;
	int fd_in;
	int fd_out;
	int fd_new_in;
	int fd_new_out;
	char buffer[BUF_SIZE];
	int nread, nwrite;

	static struct option long_options[] = {
		{"infile=",		required_argument,		NULL,		'i'},
		{"outfile=",	required_argument,		NULL,		'o'},
		{"segfault",	no_argument,			NULL,		's'},
		{"catch",		no_argument,			NULL,		'c'},
		{NULL, 0,	NULL, 0}
	};

	while((ch=getopt_long(argc, argv, ":i:o:sc", long_options, NULL))!=-1){
		switch(ch)
		{
			case 'i':
				fd_in = open(optarg, O_RDONLY);
				if(fd_in < 0){
					perror("Error opening");
					exit(1);
				}
				else{
					close(0);
					fd_new_in = dup(fd_in);
					close(fd_in);
					/*
					if((nread=read(0, buffer, BUF_SIZE))<0){
						fprintf(stderr, "Error on reading %s\n", optarg);
						perror("Error reading");
						exit(4);
					}*/
					break;
				}
				
			
			case 'o':
				fd_out = creat(optarg, 0666);
				if(fd_out<0){
					perror("Error creating");
					exit(2);
				}
				else{
					close(1);
					fd_new_out = dup(fd_out);
					close(fd_out);
					/*
					nwrite = write(1, buffer, nread);
					if(nwrite != nread){
						perror("Error on writing");
						exit(5);
					}*/
					
					break;
				}
				
			
			case 's':
				printf("creating segfault...\n");
				char* c = NULL;
				*c = 'q';
			
			case 'c':
				(void) signal(SIGSEGV, sig_handler);
				break;


		}


		
	}
	argc -= optind;
	argv += optind;
	if((nread=read(0, buffer, BUF_SIZE))<0){
			perror("Error reading");
			exit(4);
	}
	
	nwrite = write(1, buffer, nread);
	if(nwrite != nread){
			perror("Error on writing");
			exit(5);
		}
	exit(1);

}

void sig_handler(int signum){
	fprintf(stderr,"Error caught on segfault\n");
	exit(3);
}
