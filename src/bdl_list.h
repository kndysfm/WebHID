/**
 * Bi-Directional Linked List module 
 */

#ifndef _BDL_LIST_H_
#define _BDL_LIST_H_

/**
 *  Type of the list and its node are pointers to struct
 */
struct _bdl_list;
typedef struct _bdl_list * bdl_list_t;
struct _bdl_list_node;
typedef struct _bdl_list_node * bdl_list_node_t;

/**
 *  Create a new list
 */
bdl_list_t bdl_list_create(void);

/**
 *  Destroy list with a function to release content 
 */
void bdl_list_destroy(bdl_list_t ls, void (*release_content) (void *));

/**
 *  Get a head node of list
 */
bdl_list_node_t bdl_list_get_head(const bdl_list_t ls);

/**
 *  Get a tail node of list
 */
bdl_list_node_t bdl_list_get_tail(const bdl_list_t ls);

/**
 *  Get a next node of argument
 */
bdl_list_node_t bdl_list_get_next(const bdl_list_t ls, const bdl_list_node_t node);

/**
 *  Get a previous node of argument
 */
bdl_list_node_t bdl_list_get_prev(const bdl_list_t ls, const bdl_list_node_t node);

/**
 *  Get a pointer to content from a node of list
 *  An user should not release the content before deleting the node
 */
void *bdl_list_extract_content(const bdl_list_node_t node);

/**
 *  Add new node containin a content into a list
 */
bdl_list_node_t bdl_list_append_node(bdl_list_t ls, void *content);

/**
 *  Deletes a node from list
 *  Returns a pointer to content
 *  You are responsilble for release of the content
 */
void *bdl_list_delete_node(bdl_list_t ls, bdl_list_node_t node);

/**
 *  
 */
int bdl_list_get_size(const bdl_list_t ls);

#endif //#ifndef _BDL_LIST_H_