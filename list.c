/*************************************************************
 * Author:        Erik Andersen (most function headers by Phil Howard)
 * Filename:      list.c
 * Date Created:  Winter Term 2016
 * Modifications:
 *  Last modified 2016-05-17 by Erik Andersen <erik.andersen@oit.edu>
 *   Fixed prev pointers in all functions. Re-wrote the DeleteItemsFilter
 *   function.
 **************************************************************
 * 
 * Overview:
 *    Course grained locking Doubly Linked List implementation in C.
 * 
 *  -- See list.h for function header blocks
 *
 ************************************************************/
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
    pthread_mutex_t lock;
} list_t;

static int Remove_From_Beginning_Prelocked(linked_list_t l, int* data);

//********************************************
linked_list_t* Init_List()
{
    list_t* list = (list_t*)malloc(sizeof(list_t));
    if (list == NULL)
    {
        return NULL;
    }

    pthread_mutex_init(&(list->lock), NULL);
    
    pthread_mutex_lock(&(list->lock));
    list->head = NULL;
    list->tail = NULL;
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
    while (NULL != list->head)
    {
        result = Remove_From_Beginning_Prelocked(l, NULL);
        if (result != 0)
        {
            return result;
        }
    }
    pthread_mutex_unlock(&(list->lock));
    pthread_mutex_destroy(&(list->lock));

    free(list);
    return 0;
}

//********************************************
static int Remove_From_Beginning_Prelocked(linked_list_t l, int* data)
{
    item_t *item;
    list_t *list = (list_t *)l;
    
    if (list->head == NULL)
    {
        return LL_LIST_EMPTY;
    }
    
    item = list->head;
    list->head = item->next;
    if (list->head == NULL)
    {
        list->tail = NULL;
    }
    else
    {
        list->head->prev = NULL;
    }
    
    if (data != NULL)
    {
        *data = item->data;
    }
    
    free(item);
    
    return 0;
}

//********************************************
int Insert_At_Beginning(linked_list_t l, int data)
{
    item_t *item;
    list_t *list = (list_t *)l;
    
    item = (item_t *)malloc(sizeof(item_t));
    if (item == NULL)
    {
        return LL_OUT_OF_MEMORY;
    }

    item->data = data;
    item->prev = NULL;
    pthread_mutex_lock(&(list->lock));
    item->next = list->head;

    if (item->next != NULL)
    {
        item->next->prev = item;
    }
    else
    {
        list->tail = item;
    }

    list->head = item;

    pthread_mutex_unlock(&(list->lock));

    return 0;
}

//********************************************
int Remove_From_Beginning(linked_list_t l, int* data)
{
    item_t *item;
    list_t *list = (list_t *)l;

    if (list->head == NULL)
    {
        return LL_LIST_EMPTY;
    }

    pthread_mutex_lock(&(list->lock));
    item = list->head;
    list->head = item->next;
    if (list->head == NULL)
    {
        list->tail = NULL;
    }
    else
    {
        list->head->prev = NULL;
    }

    if (data != NULL)
    {
        *data = item->data;
    }

    free(item);

    pthread_mutex_unlock(&(list->lock));

    return 0;
}

//********************************************
int Traverse(linked_list_t l, void (*action)(int value, void * userData),
             void * userData)
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
int DeleteItemsFilter(linked_list_t l,
                      int (*deleteTest)(int value, void * userData),
                      void * userData)
{
    int removedCount = 0;
    item_t * item;
    list_t *list = (list_t *)l;
    
    pthread_mutex_lock(&(list->lock));
    item = list->head;
    while (item != NULL)
    {
        if (deleteTest(item->data, userData))
        {
            // We need to remove this item
            ++removedCount;
            item_t * toRemove = item;
            
            // At the start or not
            if (NULL == item->prev)
            {
                // At the start
                // No node before us -- removed start of the list, update head
                list->head = item->next;
            }
            else
            {
                // Not at the start
                // Node before us is a real node, not head
                item->prev->next = item->next;
            }
            
            if (item->next)
            {
                item->next->prev = item->prev;
            }
            else
            {
                // At end of list
                list->tail = item->prev;
            }
            
            item = item->next;
            free(toRemove);
        }
        else
        {
            item = item->next;
        }
    }
    pthread_mutex_unlock(&(list->lock));
    
    return removedCount;
}