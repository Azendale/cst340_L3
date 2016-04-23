#pragma once
//*************************************************
// linked list definition
//
// This header files defines types and functions for a linked list used to 
// store key-value pairs.
//
// Author: Philip Howard
// Date:   2016/02/20
//

#include <stdint.h>

// Error returns
#define LL_OUT_OF_MEMORY    1
#define LL_NOT_IMPLEMENTED  2
#define LL_LIST_EMPTY       3
#define LL_INVALID_ITER     4

// Opaque type for lists
typedef void *linked_list_t;

// Create and initialize a list. 
// Return pointer to list. Return NULL on failure.
linked_list_t* Init_List();

// Delete a list are free all memory used by the list
// It is erroneous to use the list pointer after caling this routine.
// Return zero on success
int Delete_List(linked_list_t list);

// Return the size of the list
// Params:
//    list: the list to return the size of
int64_t Count(linked_list_t list);

// Insert an item at the beginning of the list
// Return zero on success
// Params:
//    list: list to add item to
//    data: value to be stored in the list
int Insert_At_Beginning(linked_list_t list, int64_t data);

// Insert an item at the end of the list
// Return zero on success
// Params:
//    list: list to add item to
//    data: value to be stored in the list
int Insert_At_End(linked_list_t list, int64_t data);

// Return true (non-zero) if the list is empty
//    list: list to examine
int Empty(linked_list_t list);

// Remove an item from the beginning of the list 
// Return zero on success
// Params:
//    list: list to remove item from
//    data: pointer to location to store data of removed item
//          if data is NULL, data is not returned
int Remove_From_Beginning(linked_list_t list, int64_t* data);

// Remove an item from the end of the list 
// Return zero on success
// Params:
//    list: list to remove item from
//    data: pointer to location to store data of removed item
//          if data is NULL, data is not returned
int Remove_From_End(linked_list_t list, int64_t *data);

// Iterate through the list. Call a function on the data from each node.
// Return zero on success
// Params:
//    list: list to traverse
//    action: The function to call for each node
//         data: The data stored at the node being acted on
int Traverse(linked_list_t list, void (*action)(int64_t data));

// Insert a value into the list in sorted order
// This function performs an insertion sort
// Return zero on success
// Params:
//    list: List to insert the value into
//    value: The value to be inserted
int Insert_In_Order(linked_list_t list, int64_t value);
