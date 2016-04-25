#pragma once


// Error returns
#define LL_OUT_OF_MEMORY    1
#define LL_LIST_EMPTY 3

// Opaque type for lists
typedef void *linked_list_t;

// Create and initialize a list. 
// Return pointer to list. Return NULL on failure.
linked_list_t* Init_List();

// Delete a list are free all memory used by the list
// It is erroneous to use the list pointer after caling this routine.
// Return zero on success
int Delete_List(linked_list_t list);

// Insert an item at the beginning of the list
// Return zero on success
// Params:
//    list: list to add item to
//    data: value to be stored in the list
int Insert_At_Beginning(linked_list_t list, int data);

// Remove an item from the beginning of the list 
// Return zero on success
// Params:
//    list: list to remove item from
//    data: pointer to location to store data of removed item
//          if data is NULL, data is not returned
int Remove_From_Beginning(linked_list_t list, int* data);

// Iterate through the list. Call a function on the data from each node.
// Return zero on success
// Params:
//    list: list to traverse
//    action: The function to call for each node
//         data: The data stored at the node being acted on
//         userData: opaque pointer for any data the user supplied function may need
int Traverse(linked_list_t list, void (*action)(int data, void * userData), void * userData);

// Iterate through the list. Call a function on the data from each node
// to decide if we should remove the node, and remove the node if the
// function returns a non-zero value.
// Returns count of items deleted
// Params:
//    list: list to traverse
//    deleteTest: The function to call for each node
//         data: The data stored at the node being acted on
//         userData: opaque pointer for any data the user supplied function may need
int DeleteItemsFilter(linked_list_t list, int (*deleteTest)(int data, void * userData), void * userData);
