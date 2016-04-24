#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include "list.h"

//********************************************
// typedef for an element of the list
typedef struct item_s
{
    int data;
    struct item_s *next;
    struct item_s *prev;
} item_t;

//********************************************
// typedef for the actual list
typedef struct list_s
{
    item_t* head;
    item_t* tail;
    int count;
    pthread_mutex_t lock;
} list_t;

static int Remove_From_Beginning_Prelocked(linked_list_t l, int* data);

//********************************************
linked_list_t* Init_List()
{
    list_t* list = (list_t*)malloc(sizeof(list_t));
    if (list == NULL) return NULL;

    pthread_mutex_init(&(list->lock), NULL);
    
    pthread_mutex_lock(&(list->lock));
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    pthread_mutex_unlock(&(list->lock));

    return (linked_list_t *)list;
}

//********************************************
int Delete_List(linked_list_t l)
{
    list_t *list = (list_t *)l;
    int result;

    pthread_mutex_lock(&(list->lock));
    // loop through and delete all elements of the list
    while (!Empty(list))
    {
        result = Remove_From_Beginning_Prelocked(l, NULL);
        if (result != 0) return result;
    }
    pthread_mutex_unlock(&(list->lock));
    pthread_mutex_destroy(&(list->lock));

    free(list);
    return 0;
}

//********************************************
// const, so no lock needed
int Count(linked_list_t l)
{
    list_t *list = (list_t *)l;
    return list->count;
}

//********************************************
int Insert_At_Beginning(linked_list_t l, int data)
{
    item_t *item;
    list_t *list = (list_t *)l;
    
    item = (item_t *)malloc(sizeof(item_t));
    if (item == NULL)
    {
        pthread_mutex_unlock(&(list->lock));
        return LL_OUT_OF_MEMORY;
    }

    item->data = data;
    item->prev = NULL;
    pthread_mutex_lock(&(list->lock));
    item->next = list->head;

    if (item->next != NULL) item->next->prev = item;

    list->head = item;
    if (list->tail == NULL) list->tail = item;

    ++(list->count);
    pthread_mutex_unlock(&(list->lock));

    return 0;
}
//********************************************
static int Insert_At_End_Prelocked(linked_list_t l, int data)
{
    item_t *item;
    list_t *list = (list_t *)l;
    
    item = (item_t *)malloc(sizeof(item_t));
    if (item == NULL) return LL_OUT_OF_MEMORY;
    
    item->data = data;
    item->next = NULL;
    item->prev = list->tail;
    
    if (item->prev != NULL) item->prev->next = item;
    
    list->tail = item;
    if (list->head == NULL) list->head = item;
    
    list->count++;
    
    return 0;
}
//********************************************
int Insert_At_End(linked_list_t l, int data)
{
    item_t *item;
    list_t *list = (list_t *)l;
    
    item = (item_t *)malloc(sizeof(item_t));
    if (item == NULL) return LL_OUT_OF_MEMORY;

    item->data = data;
    item->next = NULL;
    pthread_mutex_lock(&(list->lock));
    item->prev = list->tail;

    if (item->prev != NULL) item->prev->next = item;

    list->tail = item;
    if (list->head == NULL) list->head = item;

    list->count++;
    pthread_mutex_unlock(&(list->lock));

    return 0;
}
//********************************************
// const, no locking needed
int Empty(linked_list_t l)
{
    list_t *list = (list_t *)l;
    if (list->head == NULL)
        return 1;
    else
        return 0;
}
//********************************************
static int Remove_From_Beginning_Prelocked(linked_list_t l, int* data)
{
    item_t *item;
    list_t *list = (list_t *)l;
    
    if (list->head == NULL) return LL_LIST_EMPTY;
    
    item = list->head;
    list->head = item->next;
    if (list->head == NULL) list->tail = NULL;
    
    if (data != NULL) *data = item->data;
    
    free(item);
    
    list->count--;
    
    return 0;
}
//********************************************
int Remove_From_Beginning(linked_list_t l, int* data)
{
    item_t *item;
    list_t *list = (list_t *)l;

    if (list->head == NULL) return LL_LIST_EMPTY;

    pthread_mutex_lock(&(list->lock));
    item = list->head;
    list->head = item->next;
    if (list->head == NULL) list->tail = NULL;

    if (data != NULL) *data = item->data;

    free(item);

    list->count--;
    pthread_mutex_unlock(&(list->lock));

    return 0;
}
//********************************************
int Remove_From_End(linked_list_t l, int* data)
{
    item_t *item;
    list_t *list = (list_t *)l;

    if (list->tail == NULL) return LL_LIST_EMPTY;

    pthread_mutex_lock(&(list->lock));
    item = list->tail;
    list->tail = item->prev;
    if (list->tail == NULL) list->head = NULL;

    if (data != NULL) *data = item->data;

    free(item);

    list->count--;
    pthread_mutex_unlock(&(list->lock));

    return 0;
}
//********************************************
int Traverse(linked_list_t l, void (*action)(int value, void * userData), void * userData)
{
    item_t *item;
    list_t *list = (list_t *)l;

    pthread_mutex_lock(&(list->lock));
    item = list->head;
    while (item != NULL)
    {
        action(item->data, userData);
        item = item->next;
    }
    pthread_mutex_unlock(&(list->lock));

    return 0;
}

//********************************************
int DeleteItemsFilter(linked_list_t l, int (*deleteTest)(int value, void * userData), void * userData)
{
	item_t ** itemReference;
	list_t *list = (list_t *)l;
	
	pthread_mutex_lock(&(list->lock));
	itemReference = &(list->head);
	while (*itemReference != NULL)
	{
		if (deleteTest((*itemReference)->data, userData))
		{
			// We need to remove this item
			item_t * removedNode = (*itemReference);
			*itemReference = (*itemReference)->next;
			free(removedNode);
		}
		else
		{
			// Think we don't need to do anything
		}
		itemReference = &((*itemReference)->next);
	}
	pthread_mutex_unlock(&(list->lock));
	
	return 0;
}



//********************************************
int Insert_In_Order(linked_list_t l, int data)
{
    item_t *item;
    item_t *new_item;
    list_t *list = (list_t *)l;
    
    pthread_mutex_lock(&(list->lock));
    // Find the spot to insert the item
    item = list->head;
    while (item != NULL && item->data < data)
    {
        item = item->next;
    }

    if (item == NULL) 
    {
        // fell off the end while searching, so insert at end
        Insert_At_End_Prelocked(l, data);
    }
    else
    {
        new_item = (item_t *)malloc(sizeof(item_t));
        if (new_item == NULL)
        {
            pthread_mutex_unlock(&(list->lock));
            return LL_OUT_OF_MEMORY; 
        }
        new_item->data = data;

        if (item->prev != NULL)
        {
            new_item->prev = item->prev;
            item->prev->next = new_item;
        }
        else
        {
            new_item->prev = NULL;
            list->head = new_item;
        }
        new_item->next = item;
        item->prev = new_item;

        list->count++;
    }
    pthread_mutex_unlock(&(list->lock));

    return 0;
}
