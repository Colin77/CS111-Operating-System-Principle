#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <time.h>
#include "SortedList.h"

#define KEY_LEN 3

int num_iteration = 1;
int num_threads = 1;
int opt_yield = 0;
int num_elements = 1;
char sync_opt;

//mutex
pthread_mutex_t mutex_lock;

//spinlock
volatile int spinlock = 0;


SortedList_t* head_ptr;
SortedListElement_t *element_array;

void *insert_delete(void* arg);

int main(int argc, char **argv)
{
	
	int ch;
	int i, j; //loop iterators
	char* rand_key;
	struct timespec start_time, end_time;
	long running_time, running_time_per_op, new;
	int rc; //return from pthread_creat
	int* pick_ind;
	int final_length;

	static struct option long_options[] = {
		{"iterations", 		required_argument,		NULL,		'i'},
		{"threads",			required_argument,		NULL,		't'},
		{"yield",			required_argument,		NULL,		'y'},
		{"sync",			required_argument,		NULL,		'S'},
		{NULL, 0, NULL, 0}
	};
	while((ch=getopt_long(argc, argv, "i:t:y:S:", long_options, NULL))!=-1){
		switch(ch)
		{
			case'i':
				num_iteration = atoi(optarg);
			break;

			case't':
				num_threads = atoi(optarg);
			break;
			
			case'y':
				opt_yield = 1;
				for(i=0; i<strlen(optarg); i++){
					if(optarg[i] == 'i')
						opt_yield |= INSERT_YIELD;
					else if(optarg[i] == 'd')
						opt_yield |= DELETE_YIELD;
					else if(optarg[i] =='l')
						opt_yield |= LOOKUP_YIELD;
				}
			break;

			case'S':
				if(optarg[0] == 'm'){
					sync_opt = 'm';
					if(pthread_mutex_init(&mutex_lock, NULL)!=0){
            			printf("mutex init failed!\n");
            			exit(EXIT_FAILURE);
            		}
            	}
            	else if(optarg[0] == 's'){
            		sync_opt = 's';
            	}
			break;	
		}
	}

	num_elements = num_threads * num_iteration;

	//creating pick indexes for each thread 
	pick_ind = (int *)malloc(sizeof(int)*num_elements);
	for(i=0; i<num_threads; i++){
		pick_ind[i] = i * num_iteration;
	}



	//Initialzing an empty list
	head_ptr =(SortedList_t *)malloc(sizeof(SortedList_t));
	if (head_ptr == NULL){
		perror("Failed!");
		exit(EXIT_FAILURE);
	}
	head_ptr->key = NULL;
	head_ptr->next = head_ptr;
	head_ptr->prev = head_ptr;

	//Initialzing all the elements
	element_array =(SortedListElement_t *) malloc(sizeof(SortedListElement_t) * num_elements);
	//Generating random keys
	for(i=0; i<num_elements; i++){
		rand_key = (char *) malloc(sizeof(char) * (KEY_LEN+1));
		for(j=0; j<KEY_LEN; j++){
			rand_key[j] = rand()%26 + 'A';
		}
		element_array[i].key = rand_key;
	}

	clock_gettime(CLOCK_MONOTONIC, &start_time);

	//initiating threads
	pthread_t* thread_id = malloc(sizeof(pthread_t)*num_threads);
	for(i = 0; i<num_threads; i++){
		rc = pthread_create(&thread_id[i], NULL, insert_delete, (void *)&pick_ind[i]);
		if(rc){
			perror("Error on creating threads!");
			exit(EXIT_FAILURE);
		}
	}

	//join for the thread
	for(i=0; i<num_threads; i++){
		rc = pthread_join(thread_id[i], NULL);
		if(rc){
			perror("Error on joinning the thread.");
			exit(EXIT_FAILURE);
		}
	}

	final_length = SortedList_length(head_ptr);
	clock_gettime(CLOCK_MONOTONIC, &end_time);

	running_time = (end_time.tv_sec * 1000000000L + end_time.tv_nsec)-(start_time.tv_sec * 1000000000L + start_time.tv_nsec);
	running_time_per_op = running_time/num_elements;
	new = running_time_per_op/(num_iteration/4);


	printf("Threads: %d, Iterations: %d, Operations: %d, Total run time: %ld time/operation: %ld, final length: %d\n", num_threads, num_iteration, num_elements, running_time, new, final_length);
	
	free(head_ptr);
	free(rand_key);
	free(element_array);
	free(pick_ind);
	free(thread_id);
	exit(0);
}



//////////thread function/////////////////////////
void *insert_delete(void* arg)
{
	int j;
	int thread_index = *((int*)arg);
	int length;
	SortedListElement_t* found_element;

	switch(sync_opt)
	{	
		case 'm':
			for(j = 0; j<num_iteration; j++){
				pthread_mutex_lock(&mutex_lock);
				SortedList_insert(head_ptr, &element_array[j+thread_index]);
				pthread_mutex_unlock(&mutex_lock);
			}
		break;

		case 's':
			for(j = 0; j<num_iteration; j++){
				while(__sync_lock_test_and_set(&spinlock, 1))
					; //while the old value is one, spin to wait to acquire the lock
				SortedList_insert(head_ptr, &element_array[j+thread_index]);
				__sync_lock_release(&spinlock);
			}
		break;
		
		default:
			for(j = 0; j<num_iteration; j++){
				SortedList_insert(head_ptr, &element_array[j+thread_index]);
			}
	}
			
	switch(sync_opt)
	{	
		case 'm':	
			pthread_mutex_lock(&mutex_lock);
			length = SortedList_length(head_ptr);
			pthread_mutex_unlock(&mutex_lock);
		break;

		case 's':
			while(__sync_lock_test_and_set(&spinlock, 1))
				;
			length = SortedList_length(head_ptr);
			__sync_lock_release(&spinlock);
		break;

		default:
			length = SortedList_length(head_ptr);
	}
 			
 	switch(sync_opt)	
 	{	
 		case 'm':
 			for(j= 0; j<num_iteration; j++){
				pthread_mutex_lock(&mutex_lock);
				found_element = SortedList_lookup(head_ptr, element_array[j+thread_index].key);
				if(found_element == NULL){
					fprintf(stderr, "Element not found, list is corrupted\n");
					exit(EXIT_FAILURE);
				}
				SortedList_delete(found_element);
				pthread_mutex_unlock(&mutex_lock);
			}
		break;

		case 's':
			for(j= 0; j<num_iteration; j++){
				while(__sync_lock_test_and_set(&spinlock, 1))
					;
				found_element = SortedList_lookup(head_ptr, element_array[j+thread_index].key);
				if(found_element == NULL){
					fprintf(stderr, "Element not found, list is corrupted\n");
					exit(EXIT_FAILURE);
				}
				SortedList_delete(found_element);
				__sync_lock_release(&spinlock);
			}
		break;
		
		default:
			for(j= 0; j<num_iteration; j++){
				found_element = SortedList_lookup(head_ptr, element_array[j+thread_index].key);
				if(found_element == NULL){
					fprintf(stderr, "Element not found, list is corrupted\n");
					exit(EXIT_FAILURE);
				}
				SortedList_delete(found_element);
			}
	}		
	//exit the thead for join
	pthread_exit(NULL);
}
