#include "list.h"

typedef struct 
{
	linked_list_t connections;
} thread_data_t;

int main(int argc, char ** argv)
{
	linked_list_t connections = Init_List();
	if (NULL == connections)
	{
		return 1;
	}
	
	

}