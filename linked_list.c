#include "linked_list.h"

// Creates and returns a new list
list_t* list_create()
{
    list_t* list = (list_t *) malloc(sizeof(list_t));
    // Initialize all fields of list_t struct
    list->head = NULL;
    list->count = 0;
    return list;
}

// Destroys a list
void list_destroy(list_t* list)
{
    free(list);
}

// Returns beginning of the list
list_node_t* list_begin(list_t* list)
{
    return list->head;
}

// Returns next element in the list
list_node_t* list_next(list_node_t* node)
{
    return node->next;
}

// Returns data in the given list node
void* list_data(list_node_t* node)
{
    return node->data;
}

// Returns the number of elements in the list
size_t list_count(list_t* list)
{
    return list->count;
}

// Finds the first node in the list with the given data
// Returns NULL if data could not be found
list_node_t* list_find(list_t* list, void* data)
{
    list_node_t* p = list->head;
    while (p != NULL) {
        if (p->data == data) //find first matched data
            return p;
        p = p->next;
    }
    return NULL;
}

// Inserts a new node in the list with the given data
void list_insert(list_t* list, void* data)
{   //add the new node to the beginning of the list
    list->count += 1;
    list_node_t* new_node = (list_node_t *) malloc(sizeof(list_node_t));
    new_node->data = data;
    new_node->prev = NULL;
    list_node_t* first_node = list->head;
    new_node->next = list->head;
    list->head = new_node;
    if (first_node != NULL)
        first_node->prev = new_node;
}

// Removes a node from the list and frees the node resources
void list_remove(list_t* list, list_node_t* node)
{
    list->count -= 1;
    if (node->prev != NULL) //not the first node in the list
        node->prev->next = node->next;
    else //the fist node in the list
        list->head = node->next;

    if (node->next != NULL) //not the last node in the list
        node->next->prev = node->prev;
    free(node);
}

// Executes a function for each element in the list
void list_foreach(list_t* list, void (*func)(void* data))
{
    list_node_t* p = list->head;
    while (p != NULL) {
        func(p->data);
        p = p->next;
    }
}
