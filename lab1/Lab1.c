#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>


#define MAX_FD 100
#define MAX_P 50
#define MAX_CHILDREN 100

int index_element(int target_id, int arr[], int num_elements)
{
	int i;
	int index;
	for (i = 0; i<num_elements; i++){
		if(arr[i] == target_id){
			return i;
		}
	}

	return -1;
}

void delete_id(int target_id, int arr[], int num_elements)
{
	int i;
	int index;
	index = index_element(target_id, arr,num_elements);
	for(i = index; i<num_elements-1; i++){
		arr[i] = arr[i+1];
	}
}

void N_handler(int signum)
{
	fprintf(stderr, "%d caught\n", signum);
	exit(signum);
}

int main(int argc, char **argv)
{
	int ch;	//For getopt cases
	int pipefd[2]; //For pipe option
	int pid;
	int quit_pid;

	int *pid_array = malloc(MAX_P * sizeof(int)); //array of child pids
	int num_running_child = 0;

	int status;  //To record the exit status of each child process
	int verbose_flag = 0;
	int o_flag = 0; //For open() flags
	int *fd_array = malloc(MAX_FD * sizeof(int));
	int num_exist_file_fd = 0;
	int temp_fd;
	char* command_args[3];
	int int_command_args[3];
	int i;
	int a;
	int b;
	char *cmd;
	char *quit_cmd;
	char* cmd_args[15]; //For args of cmd that will be invoked
	int num_cmd_arg; //number of cmd args;
	char* exec_args[16];
	int k;
	int sig_num;
	int fd_tobe_close;

	struct child_info
	{
		int id;
		char* command;
		int exit_code;
		char* child_args;
	};

	struct child_info children[MAX_CHILDREN];

	static struct option long_options[] = {
		{"rdonly",		required_argument, 		NULL,		'R'},
		{"wronly",		required_argument,		NULL,		'W'},
		{"rdwr",		required_argument, 		NULL,		'+'},
		{"command",		required_argument,		NULL,		'c'},
		{"verbose",		no_argument,			NULL,		'v'},
		{"pipe",		no_argument,			NULL,		'p'},
		{"wait",		no_argument,			NULL,		'z'},
		{"append",		no_argument,			NULL,		'1'},
		{"cloexec",		no_argument,			NULL,		'2'},
		{"creat",		no_argument,			NULL,		'3'},
		{"diretory",	no_argument,			NULL,		'4'},
		{"dsync",		no_argument,			NULL,		'5'},
		{"excl",		no_argument,			NULL,		'6'},
		{"nofollow",	no_argument,			NULL,		'7'},
		{"nonblock",	no_argument,			NULL,		'8'},
		{"rsync",		no_argument,			NULL,		'9'},
		{"sync",		no_argument,			NULL,		'0'},
		{"trunc",		no_argument,			NULL,		't'},
		{"abort",		no_argument,			NULL,		'A'},
		{"pause",		no_argument,			NULL,		'P'},
		{"default",		required_argument,		NULL,		'd'},
		{"ignore",		required_argument,		NULL,		'i'},
		{"catch",		required_argument,		NULL,		'C'},
		{"close",		required_argument,		NULL,		'g'},
		{NULL,	0,	NULL,	0}
	};

	while((ch=getopt_long(argc, argv, ":R:W:c:vpz1234567890t+:AP", long_options, NULL))!=-1){
		switch(ch)
		{
			//close file
			case 'g':
				fd_tobe_close = atoi(optarg);
				if((close(fd_array[fd_tobe_close])) == -1){
					perror("Error closing the file");
				}
				
			case 'C':
				sig_num = atoi(optarg);
				(void) signal(sig_num, N_handler);
				break;
			break;
			
			case 'd':
				sig_num = atoi(optarg);
				(void) signal(sig_num, SIG_DFL);
				break;
			break;

			case 'i':
				sig_num = atoi(optarg);
				(void) signal(sig_num, SIG_IGN);
				break;

			/*Start of open flag configuration*/
			case '1':
				o_flag = o_flag | O_APPEND;
			break;
			case '2':
				o_flag = o_flag | O_CLOEXEC;
			break;
			case '3':
				o_flag = o_flag | O_CREAT;
			break;
			case '4':
				o_flag = o_flag | O_DIRECTORY;
			break;
			case '5':
				o_flag = o_flag | O_DSYNC;
			break;
			case '6':
				o_flag = o_flag | O_EXCL;
			break;
			case '7':
				o_flag = o_flag | O_NOFOLLOW;
			break;
			case '8':
				o_flag = o_flag | O_NONBLOCK;
			break;
			case '9':
				//o_flag = o_flag | O_RSYNC;   is not supportted by Mac
			break;
			case '0':
				o_flag = o_flag | O_SYNC;
			break;
			case 't':
				o_flag = o_flag | O_TRUNC;
			break;
			/*End of open flag configuration*/
			
			//--wait case
			case 'z':
				//first close all the fds
				for (k = 0; k<num_exist_file_fd; k++){
					close(fd_array[num_exist_file_fd]);
				}
				while(num_running_child!=0){
					quit_pid = wait(&status);
					printf("Deleting Child with ID %d now...\n", quit_pid);
					delete_id(quit_pid, pid_array, num_running_child);
					//find and print out the corresponding child info
					for(i = 0; i<num_running_child; i++){
						if(children[i].id == quit_pid){
							quit_cmd = children[i].command;
						}
					}
					printf("Quitting command: %s, exit status: %d\n", quit_cmd, WEXITSTATUS(status));
					num_running_child = num_running_child -1;
				}
			break;

			//abort
			case 'A':
				printf("Aborting the system now...\n");
				char* staff = NULL;
				*staff = 'A';
			break;
			
			//pause
			case 'P':
				pause();
			break;

			case '+':
				o_flag = o_flag | O_RDWR;
				temp_fd = open(optarg, o_flag);
				if(temp_fd < 0){
					perror("Cannot open the file");
					exit(EXIT_FAILURE);
				}
				else{
					num_exist_file_fd = num_exist_file_fd + 1;
					//if current array is enough or not?
					if(num_exist_file_fd < MAX_FD){
					fd_array[num_exist_file_fd-1] = temp_fd;
					}
					else{
						int *larger_fd = realloc(fd_array, 100*sizeof(int));
						if(!larger_fd){
							perror("Not enough memory");
							exit(EXIT_FAILURE);
						}
						fd_array = larger_fd;
						fd_array[num_exist_file_fd-1] =temp_fd;
					}
				}
				break;		
			
			case 'R':
				o_flag = o_flag | O_RDONLY;
				temp_fd = open(optarg, o_flag);
				if(temp_fd < 0){
					perror("Cannot open the file");
					exit(EXIT_FAILURE);
				}	
				else{
					num_exist_file_fd = num_exist_file_fd + 1;
					//if current array is enough or not?
					if(num_exist_file_fd < MAX_FD){
					fd_array[num_exist_file_fd-1] = temp_fd;
					}
					else{
						int *larger_fd = realloc(fd_array, 100*sizeof(int));
						if(!larger_fd){
							perror("Not enough memory");
							exit(EXIT_FAILURE);
						}
						fd_array = larger_fd;
						fd_array[num_exist_file_fd-1] =temp_fd;
						o_flag = 0;
					}
				}				
			break;

			case 'W':
				o_flag = o_flag | O_WRONLY;
				temp_fd = open(optarg, o_flag);
				if(temp_fd < 0){
					perror("Cannot open the file");
					exit(EXIT_FAILURE);
				}
				else{
					num_exist_file_fd = num_exist_file_fd + 1;
					//if current array is enough or not?
					if(num_exist_file_fd < MAX_FD){
					fd_array[num_exist_file_fd-1] = temp_fd;
					}
					else{
						int *larger_fd = realloc(fd_array, 100*sizeof(int));
						if(!larger_fd){
							perror("Not enough memory");
							exit(EXIT_FAILURE);
						}
						fd_array = larger_fd;
						fd_array[num_exist_file_fd-1] =temp_fd;
						o_flag = 0;
					}
				}			
			break;
			
			case 'v':
				printf("--verbose option activated\n");
				verbose_flag = 1;
			break;

			case 'c':
				num_running_child = num_running_child + 1;
				//Let the optind indicate the first arg of --command
				optind = optind - 1;

				//Store three args of --command into array
				for(i=0; i<3; i=i+1){
					command_args[i]=argv[optind];
					//next arg index
					optind = optind+1;
				}
				//command string args to int for later use
				for(i=0; i<3; i=i+1){
					int_command_args[i]=atoi(command_args[i]);
					if(int_command_args[i] >= num_exist_file_fd){
						perror("Invalid file descriptor");
						exit(EXIT_FAILURE);
					}
				}

				//Get the command name
				cmd = argv[optind];
				//iterating to get the cmd args and args number
				a = 0;
				for(i=optind+1; i<argc; i++){
					if(strstr(argv[i], "--") != NULL){ //Check for substring
						break;
					}
					cmd_args[a] = argv[i];
					a = a+1;
				}
				//Clear out the possible redundent args from previous call to command 
				cmd_args[argc-optind-1]= NULL;
				
				//printing out options
				if(verbose_flag == 1){
					printf("Option: command; operand: %s ", cmd);
					for(i = 0; cmd_args[i]!= NULL; i++){
						printf("%s ",cmd_args[i]);
					}
					printf("\n");
				}
			
				//fork to excecute cmd
				pid = fork();
				if(pid == -1){
					perror("Cannot create new process");
					exit(EXIT_FAILURE);
				}
				
				if(pid == 0){
					//I/O redirection
					dup2(fd_array[int_command_args[0]], 0);
					close(fd_array[int_command_args[0]]);
					dup2(fd_array[int_command_args[1]],1);
					close(fd_array[int_command_args[1]]);
					dup2(fd_array[int_command_args[2]],2);
					close(fd_array[int_command_args[2]]);

		        	//Setting up the args for exec
					exec_args[0] = cmd;
					for(i=1; cmd_args[i-1]!=NULL; i++){
						exec_args[i] = cmd_args[i-1];
					}
					//Indicate the end of the arg array
					exec_args[i] = NULL;

					//excecuting command
					execvp(cmd, exec_args);
				}

				if(pid > 0){ //parent continue to process more command
					//Storing the pid of child process into a array
					pid_array[num_running_child-1] = pid;
					
					//Store the child process info into a struct
					children[num_running_child-1].id = pid;
					children[num_running_child-1].command = cmd;
					continue;

				}
				break;

				case 'p':
					if(pipe(pipefd)==-1){
						perror("Error creating pipe");
						exit(EXIT_FAILURE);
					}

					num_exist_file_fd = num_exist_file_fd + 2;
					fd_array[num_exist_file_fd-2] = pipefd[0];
					fd_array[num_exist_file_fd-1] = pipefd[1];

				case '?':
					//continue to next command if encounter unknow command
					break;

		}
	}

	free(fd_array);
	free(pid_array);
	exit(0);
}

