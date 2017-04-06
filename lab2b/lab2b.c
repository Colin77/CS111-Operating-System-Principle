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
int num_list = 1;

//mutex
pthread_mutex_t mutex_lock;
//array of mutexes for each list;
pthread_mutex_t* mutex_ary; 

//spinlock
volatile int spinlock = 0;

SortedList_t* head_ptr;
SortedListElement_t *element_array;
SortedList_t** list_head_ary;

//thread function
void *insert_delete(void* arg);
void *multi_insert_delete(void* arg);
//hash funciton for multiple list insertion
int hash_key(const char* key);

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
	int list_flag = 0;

	static struct option long_options[] = {
		{"iterations", 		required_argument,		NULL,		'i'},
		{"threads",			required_argument,		NULL,		't'},
		{"yield",			required_argument,		NULL,		'y'},
		{"sync",			required_argument,		NULL,		's'},
		{"lists",			required_argument,		NULL,		'l'},
		{NULL, 0, NULL, 0}
	};
	while((ch=getopt_long(argc, argv, "i:t:y:s:l:", long_options, NULL))!=-1){
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

			case's':
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

			case'l':
				list_flag = 1;
				num_list = atoi(optarg);
				//allocate multiple heads for different lists and assign struct to them
				list_head_ary = malloc(sizeof(SortedList_t*) * num_list);
				for(i=0; i<num_list; i++){
					list_head_ary[i] = malloc(sizeof(SortedList_t));
					if(list_head_ary[i] == NULL){
						perror("Error on creating space for struct");
						exit(EXIT_FAILURE);
					}
				}
				if(list_head_ary == NULL){
					perror("Error on malloc list haed array");
					exit(EXIT_FAILURE);
				}
				//initialize each list's content
				for(i=0; i<num_list; i++){
					list_head_ary[i]->key = NULL;
					list_head_ary[i]->next = list_head_ary[i];
					list_head_ary[i]->prev = list_head_ary[i];
				}
				//Up to this point we hace array of list head available: list_head_ary
			break;

		}
	}
	
	//allocate same number of mutex for each list and initialize them
	if(list_flag == 1 && sync_opt == 'm'){
		mutex_ary = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t) * num_list);
			if(mutex_ary == NULL){
				perror("Error on malloc mutex_ary");
				exit(EXIT_FAILURE);
			}
			for(i=0; i<num_list; i++){
				pthread_mutex_init(&mutex_ary[i], NULL);
			}
	}

	num_elements = num_threads * num_iteration;

	//creating pick indexes for each thread 
	pick_ind = (int *)malloc(sizeof(int)*num_elements);
	for(i=0; i<num_threads; i++){
		pick_ind[i] = i * num_iteration;
	}

	//Initialzing an empty list if no "lists option"
	if(list_flag == 0){
		head_ptr =(SortedList_t *)malloc(sizeof(SortedList_t));
		if (head_ptr == NULL){
			perror("Failed!");
			exit(EXIT_FAILURE);
		}
		head_ptr->key = NULL;
		head_ptr->next = head_ptr;
		head_ptr->prev = head_ptr;
	}	
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
	if(list_flag == 0){
		printf("NO\n");
		for(i = 0; i<num_threads; i++){
			rc = pthread_create(&thread_id[i], NULL, insert_delete, (void *)&pick_ind[i]);
			if(rc){
				perror("Error on creating threads!");
				exit(EXIT_FAILURE);
			}
		}
	}

	else if(list_flag == 1){
		printf("YES\n");
		for(i=0; i<num_threads; i++){
			rc = pthread_create(&thread_id[i], NULL, multi_insert_delete, (void *)&pick_ind[i]);
			if(rc){
				perror("Error on creating threads!");
				exit(EXIT_FAILURE);
			}
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

	if(list_flag==0){
	final_length = SortedList_length(head_ptr);
	}
	clock_gettime(CLOCK_MONOTONIC, &end_time);

	running_time = (end_time.tv_sec * 1000000000L + end_time.tv_nsec)-(start_time.tv_sec * 1000000000L + start_time.tv_nsec);
	running_time_per_op = running_time/num_elements;
	new = running_time_per_op/(num_iteration/4);

	if(list_flag == 0){
	printf("Threads: %d, Iterations: %d, Operations: %d, Total run time: %ld time/operation: %ld, final length: %d\n", num_threads, num_iteration, num_elements, running_time, new, final_length);
	}
	else if(list_flag == 1){
		int lists_len;
		for(i=0; i<num_list; i++){
			lists_len = SortedList_length(list_head_ary[i]);
			printf("The length of list %d is: %d\n", i, lists_len);
		}

		printf("Running time: %ld, time per operation: %ld\n", running_time, new);
	}
	if(list_flag == 0){
		free(head_ptr);
	}
	free(rand_key);
	free(element_array);
	free(pick_ind);
	free(thread_id);
	free(list_head_ary);
	if(list_flag == 1 && sync_opt == 'm'){
	free(mutex_ary);
	}
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
			for(j=0; j<num_iteration; j++){
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

void *multi_insert_delete(void* arg)
{
	int j;
	int thread_index = *((int *)arg);
	int length;
	SortedListElement_t* found_element;
	int hash_value;
	//stores the list number each element stored in
	int list_needed[num_iteration];
	//inserting element accroding to hash value
	switch(sync_opt)
	{
		case'm':
			for(j=0; j<num_iteration; j++){
				hash_value = hash_key(element_array[j+thread_index].key);
				list_needed[j] = hash_value;
				//lock the corrsponding list that will be accessed via hash value
				pthread_mutex_lock(&mutex_ary[hash_value]);
				SortedList_insert(list_head_ary[hash_value], &element_array[j+thread_index]);
				pthread_mutex_unlock(&mutex_ary[hash_value]);
			}

			for(j=0; j<num_list; j++){
				pthread_mutex_lock(&mutex_ary[j]);
				length = SortedList_length(list_head_ary[j]);
				pthread_mutex_unlock(&mutex_ary[j]);
			}

		break;
		
		default:
			for(j=0; j<num_iteration; j++){
				hash_value = hash_key(element_array[j+thread_index].key);
				list_needed[j] = hash_value;
				SortedList_insert(list_head_ary[hash_value], &element_array[j+thread_index]);
			}
			//get the length of the  all the list
			for(j=0; j<num_list; j++){
				length = SortedList_length(list_head_ary[j]);
			}

	}

	//lookup and delete all the element
	switch(sync_opt)
	{
		case'm':
			for(j=0; j<num_iteration; j++){
				pthread_mutex_lock(&mutex_ary[list_needed[j]]);
				found_element = SortedList_lookup(list_head_ary[list_needed[j]], element_array[j+thread_index].key);
				if(found_element == NULL){
					perror("Element not found, list corrupted!!!!!!");
					exit(EXIT_FAILURE);
				}

				SortedList_delete(found_element);
				pthread_mutex_unlock(&mutex_ary[list_needed[j]]);
			}
		break;
		default:
			for(j=0; j<num_iteration; j++){
				found_element = SortedList_lookup(list_head_ary[list_needed[j]], element_array[j+thread_index].key);
				if(found_element == NULL){
					perror("Element not found, list corrupted!!!!!!");
					exit(EXIT_FAILURE);
				}

				SortedList_delete(found_element);
			}

	}

	pthread_exit(NULL);

}

//convert string to int, then hash
int hash_key(const char* key)
{
	int i;
	int to_hash, hash_value;
	to_hash = 0;
	for(i=0; i<strlen(key); i++){
		to_hash = to_hash*10 + (key[i] - '0');
	}
	hash_value = to_hash % num_list;
	return hash_value;
}
