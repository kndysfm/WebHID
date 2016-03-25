/**
 * Bi-Directional Linked List module 
 */

#include <stdlib.h>
#include <string.h>
#include "bdl_list.h"

#ifdef _DEBUG
#include <stdio.h>
#define BDL_LIST_ASSERT(exp) \
	do { if(!(exp)) printf("%s (% 4d): [ASSERT] \"%s\" is falsy\r\n", __FUNCTION__, __LINE__, #exp); } while(0)
#define BDL_LIST_TRACE(msg) \
	printf("%s (% 4d): %s\r\n", __FUNCTION__, __LINE__, msg)
#else //_DEBUG
#define BDL_LIST_ASSERT(exp)
#define BDL_LIST_TRACE(msg)
#endif //_DEBUG

struct _bdl_list_node {
	void *content;
	struct _bdl_list_node *prev;
	struct _bdl_list_node *next;
};

struct _bdl_list {
	struct _bdl_list_node head;
	struct _bdl_list_node tail;
	size_t size;
};

bdl_list_t bdl_list_create(void) {
	bdl_list_t ls = (bdl_list_t) malloc(sizeof(struct _bdl_list));
	if (ls) {
		ls->head.content = 0;
		ls->head.next = &ls->tail;
		ls->head.prev = 0; // forever
		ls->tail.content = 0;
		ls->tail.prev = &ls->head;
		ls->tail.next = 0; // forever
		ls->size = 0;
	} else BDL_LIST_TRACE("failed to allocate memmory");

	return ls;
}

void bdl_list_destroy(bdl_list_t ls, void (*release_content) (void *)) {
	bdl_list_node_t node = ls->head.next;
	while(node != &ls->tail) {
		bdl_list_node_t next = node->next;
		node->next = node->prev = 0;
		ls->size--;
		BDL_LIST_ASSERT(ls->size >= 0); 
		release_content(node->content);
		node->content = 0;
		free(node);
		node = next;
	}

	free(ls);
}

bdl_list_node_t bdl_list_get_head(const bdl_list_t ls) {
	return ls->head.next != &ls->tail? ls->head.next: 0;
}

bdl_list_node_t bdl_list_get_tail(const bdl_list_t ls) {
	return ls->tail.prev != &ls->head? ls->tail.prev: 0; 
}

bdl_list_node_t bdl_list_get_next(const bdl_list_t ls, const bdl_list_node_t node) {
	BDL_LIST_ASSERT(node != &ls->head && node != &ls->tail);
	return node->next != &ls->tail? node->next: 0;
}

bdl_list_node_t bdl_list_get_prev(const bdl_list_t ls, const bdl_list_node_t node) { 
	BDL_LIST_ASSERT(node != &ls->head && node != &ls->tail);
	return node->prev != &ls->head? node->prev: 0; 
}

void *bdl_list_extract_content(const bdl_list_node_t node) { 
	BDL_LIST_ASSERT(node->next && node->prev);
	return node->content;
}

bdl_list_node_t bdl_list_append_node(bdl_list_t ls, void *content) {
	bdl_list_node_t node = (bdl_list_node_t) malloc(sizeof(struct _bdl_list_node));
	if (node) {
		node->content = content;
		if (ls->size == 0) {
			BDL_LIST_ASSERT(ls->head.next == &ls->tail && &ls->head == ls->tail.prev);
			node->prev = &ls->head;
			node->next = &ls->tail;
		} else {
			node->prev = ls->tail.prev;
			node->next = &ls->tail;
		}
		node->prev->next = node;
		node->next->prev = node;
		ls->size++;
	} else BDL_LIST_TRACE("failed to allocate memmory");

	return node;
}

void *bdl_list_delete_node(bdl_list_t ls, bdl_list_node_t node) {
	if (node->prev && node->next) {
		bdl_list_node_t hd = node->prev;
		bdl_list_node_t tl = node->next;

		while (hd->prev) hd = hd->prev;
		while (tl->next) tl = hd->next;
		// now, head->prev == 0 && tail->next == 0
		if (hd == &ls->head && tl == &ls->tail) {
			// node is in this list
			void *c = node->content;
			node->prev->next = node->next;
			node->next->prev = node->prev;
			ls->size--;
			BDL_LIST_ASSERT(ls->size >= 0); 

			node->content = 0;
			node->next = 0;
			node->prev = 0;
			free(node);

			return c;
		}
		else BDL_LIST_TRACE("node to delete is not found");
	} else BDL_LIST_TRACE("links of node are invalid");
	
	return 0; // invalid node
}

int bdl_list_get_size(const bdl_list_t ls) {
	return ls->size;
}
