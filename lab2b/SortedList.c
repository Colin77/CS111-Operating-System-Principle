#include "SortedList.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>

int SortedList_length(SortedList_t *list)
{
	int count = 0;
	SortedListElement_t *ptr = list->next;
	if(opt_yield & LOOKUP_YIELD){
		sched_yield();
	}
	while(ptr!=list){
		count = count+1;
		ptr = ptr->next;
	}
	return count;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key)
{
	SortedListElement_t* temp;
	temp = list->next;
	if(opt_yield & LOOKUP_YIELD){
		sched_yield();
	}
	while(temp!=list){
		if(strcmp(temp->key, key)==0)
			return temp;
		temp = temp->next;
	}
	return NULL;
}

int SortedList_delete( SortedListElement_t *element)
{
	//list is corruptted
	if(element->prev->next!=element || element->next->prev!=element)
		return 1;
	element->prev->next = element->next;
	if(opt_yield & DELETE_YIELD){
		sched_yield();
	}
	element->next->prev = element->prev;
	return 0;
}

void SortedList_insert(SortedList_t* list, SortedListElement_t* element)
{

	SortedListElement_t* current = list;
	SortedListElement_t* go = list->next;
	while(go!=list){
		if(strcmp(element->key, go->key) <= 0)
			break;
		current = go;
		go = go->next;
	}
	element->prev = current;
	if(opt_yield & INSERT_YIELD){
		sched_yield();
	}
	element->next = go;
	current->next = element;
	go->prev = element;

}
