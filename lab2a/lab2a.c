#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <pthread.h>
#include <time.h> //for clock_gettime()
#include <sched.h> //for sched_yield()
#include <string.h>

void *add(long long* pointer, long long value);
void *add_mutex(long long* pointer, long long value);
void *add_testnset(long long* pointer, long long value);
void *add_comparenswap(long long* pointer, long long value);
//wrapper function
void *add_minus(void* times);
void *add_minus_mutex(void* times);
void *add_minus_spin(void* times);
void *add_minus_cmp(void* times);

long long counter=0;
long long *cnt_pt = &counter;
int num_threads = 1;
int num_interations = 1;
int opt_yield=0;

//define global mutex variable
pthread_mutex_t lock;

//define global lock for test&set
volatile int spinlock = 0;


int main(int argc, char **argv)
{
	struct timespec timeval_start, timeval_end;
	long running_time, running_time_per_op;
	int ch;
	int i; //for loop index
	int rc; //return value of pthread_create
	int total_ops;
	int mutex_flag, spin_flag, compare_flag;
	char* opt_sync;
	mutex_flag = 0;
	spin_flag = 0;
	compare_flag = 0;


	static struct option long_options[] = {
		{"iterations", 		required_argument,		NULL,		'i'},
		{"threads",			required_argument,		NULL,		't'},
		{"yield",			no_argument,			NULL,		'y'},
		{"sync",			required_argument,		NULL,		'S'},
		{NULL, 0, NULL, 0}
	};

	//parsing the command line, set the thread & iteration number
	while((ch=getopt_long(argc, argv, "i:t:yS:", long_options, NULL))!=-1){
		switch(ch)
		{
			case'i':
				num_interations = atoi(optarg);
			break;

			case't':
				num_threads = atoi(optarg);
            break;

            case'y':
            	opt_yield = 1;
            break;

            case 'S':
            	if(strcmp(optarg, "m") == 0){
            		mutex_flag = 1;
            		opt_sync = "m";
            		//initiate mutex
            		if(pthread_mutex_init(&lock, NULL)!=0){
            			printf("mutex init failed!\n");
            			exit(EXIT_FAILURE);
            		}
            	}
            	if(strcmp(optarg, "s") == 0)
            		spin_flag = 1;
            		opt_sync = "s";
            	if(strcmp(optarg, "c")==0)
            		compare_flag=1;
            		opt_sync = "c";
            break;

		}
	}
	total_ops = num_threads * num_interations * 2;

	//start the clock
	if(clock_gettime(CLOCK_MONOTONIC, &timeval_start) == -1){
		perror("clock_gettime");
		exit(EXIT_FAILURE);
	}
	//creating threads
	pthread_t thread[num_threads];
	
	//without synchronnization
	if(mutex_flag ==0 && spin_flag ==0 && compare_flag ==0){
		printf("No sync...\n");
		for(i=0; i<num_threads; i++){
			printf("Creating thread %d\n", i);
			rc=pthread_create(&thread[i], NULL, add_minus, (void *)num_interations);
			if(rc){
				printf("Error on creating thread\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	//Using mutex
	else if(mutex_flag ==1 && spin_flag ==0 && compare_flag ==0){
		printf("mutex...\n");
		for(i=0; i<num_threads; i++){
			printf("Creating thread %d\n", i);
			rc=pthread_create(&thread[i], NULL, add_minus_mutex, (void *)num_interations);
			if(rc){
				printf("Error on creating thread\n");
				exit(EXIT_FAILURE);
			}
		}

	}

	//using spinlock
	else if(mutex_flag ==0 && spin_flag ==1 && compare_flag ==0){
		printf("spinlock...\n");
		for(i=0; i<num_threads; i++){
			printf("Creating thread %d\n", i);
			rc=pthread_create(&thread[i], NULL, add_minus_spin, (void *)num_interations);
			if(rc){
				printf("Error on creating thread\n");
				exit(EXIT_FAILURE);
			}
		}

	}

	//using compare and swap
	else if(mutex_flag ==0 && spin_flag ==0 && compare_flag ==1){
		printf("CAS...\n");
		for(i=0; i<num_threads; i++){
			printf("Creating thread %d\n", i);
			rc=pthread_create(&thread[i], NULL, add_minus_cmp, (void *)num_interations);
			if(rc){
				printf("Error on creating thread\n");
				exit(EXIT_FAILURE);
			}
		}

	}

	

	//join all the threads
	for(i=0; i<num_threads; i++){
		rc = pthread_join(thread[i], NULL);
		if(rc){
			printf("Error on joining threads\n");
			exit(EXIT_FAILURE);
		}
	}

	if(clock_gettime(CLOCK_MONOTONIC, &timeval_end) == -1){
		perror("clock_gettime");
		exit(EXIT_FAILURE);
	}
	//calculating the running time & time/op
	running_time = (timeval_end.tv_sec * 1000000000L + timeval_end.tv_nsec)-(timeval_start.tv_sec * 1000000000L + timeval_start.tv_nsec);
	running_time_per_op = running_time/total_ops;

	if(opt_yield == 0){
		printf("add-%s,%d,%d,%d,%ld,%ld,%lld\n", opt_sync, num_threads,num_interations, total_ops, running_time, running_time_per_op, counter);
	}
	else
		printf("add-%s-yield,%d,%d,%d,%ld,%ld,%lld\n", opt_sync, num_threads,num_interations, total_ops, running_time, running_time_per_op, counter);

	mutex_flag = 0;
	spin_flag = 0;
	compare_flag = 0;
	pthread_exit(NULL);
	if(mutex_flag == 1)
		pthread_mutex_destroy(&lock);
	exit(EXIT_SUCCESS);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////Funciton implementation////////////////////////////////////////////////////////////////////
void *add(long long* pointer, long long value)
{
	counter = *pointer + value;
	if(opt_yield)
		sched_yield();
	*pointer = counter;
	return NULL;
}

void *add_mutex(long long* pointer, long long value)
{
	pthread_mutex_lock(&lock);

	counter = *pointer + value;
	if(opt_yield)
		sched_yield();
	*pointer = counter;

	pthread_mutex_unlock(&lock);
	return NULL;
}

void *add_testnset(long long* pointer, long long value)
{
	while(__sync_lock_test_and_set(&spinlock, 1))
		; //spin to wait-->busy waiting
	
	counter = *pointer + value;
	if(opt_yield)
		sched_yield();
	*pointer = counter;

	__sync_lock_release(&spinlock);
	return NULL;

}

void *add_comparenswap(long long* pointer, long long value)
{
	long long current = *pointer;
	long long new = current + value;
	while(__sync_val_compare_and_swap(cnt_pt, current, new) != current)
		;
	return NULL;
}

//wrapper function for add, +1 & -1 for multiple times
void *add_minus(void* times)
{
	int k;
	int iteration = (int)times;
	for(k=0; k<iteration; k++){
		add(cnt_pt, 1);
		add(cnt_pt, -1);
	}
	//exit thread for later join() in main thread
	pthread_exit(NULL);
}

void *add_minus_mutex(void* times){
	int k;
	int iteration = (int)times;
	for(k=0; k<iteration; k++){
		add_mutex(cnt_pt, 1);
		add_mutex(cnt_pt, -1);
	}
	//exit thread for later join() in main thread
	pthread_exit(NULL);
}

void *add_minus_spin(void* times)
{
	int k;
	int iteration = (int)times;
	for(k=0; k<iteration; k++){
		add_testnset(cnt_pt, 1);
		add_testnset(cnt_pt, -1);
	}
	//exit thread for later join() in main thread
	pthread_exit(NULL);
}

void *add_minus_cmp(void* times)
{
	int k;
	int iteration = (int)times;
	for(k=0; k<iteration; k++){
		add_comparenswap(cnt_pt, 1);
		add_comparenswap(cnt_pt, -1);
	}
	pthread_exit(NULL);
}