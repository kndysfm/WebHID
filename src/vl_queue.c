/**
 *  Variable Length Queue module
 */

#include <stdlib.h>
#include <string.h>
#include "vl_queue.h"

struct vl_queue_packet {
	size_t size;
	uint8_t *data;
};

struct _vl_queue {
	size_t num_packets;
	size_t max_packets;
	struct vl_queue_packet **packets;
};

#ifdef _DEBUG
#include <stdio.h>
#define VL_QUEUE_ASSERT(exp)	\
	do { if(!(exp)) printf("%s (% 4d): [ASSERT] \"%s\" is falsy\r\n", __FUNCTION__, __LINE__, #exp); } while(0)
#define VL_QUEUE_TRACE(q)		\
	do { printf("%s (% 4d): %s = { num_packets: %d, max_packets: %d }\r\n", __FUNCTION__, __LINE__, #q, q->num_packets, q->max_packets); }while(0)
#else //_DEBUG
#define VL_QUEUE_ASSERT(exp)
#define VL_QUEUE_TRACE(q)
#endif //_DEBUG

#define VL_QUEUE_DEFAULT_SIZE (8)
#define VL_QUEUE_MAXIMUM_SIZE (64)

vl_queue_t vl_queue_create(void) {
	vl_queue_t q = (vl_queue_t) malloc(sizeof(struct _vl_queue));
	if (q)
	{
		struct vl_queue_packet **packets;
		packets = (struct vl_queue_packet **)calloc(VL_QUEUE_DEFAULT_SIZE, sizeof(struct vl_queue_packet *));

		if (packets) {
			q->num_packets = 0;
			q->max_packets = VL_QUEUE_DEFAULT_SIZE;
			q->packets = packets;
		} else {
			VL_QUEUE_TRACE(q);
			free(q);
			q = 0;
		}
	}

	return q;
}

static void vlq_free_packet(struct vl_queue_packet **pkt) {
	if ((*pkt)->data) {
		free((*pkt)->data);
		(*pkt)->data = 0;
	}
	(*pkt)->size = 0;
	free(*pkt);
	*pkt = 0;
}

void vl_queue_destroy(vl_queue_t q) {
	int num = q->num_packets;
	int i;
	q->num_packets = 0;
	for (i = 0; i < num; i++) {
		vlq_free_packet(&q->packets[i]);
	}
	free(q->packets);
	free(q);
}

static void vlq_extend(vl_queue_t q, size_t plus) {
	struct vl_queue_packet **packets_ex;
	packets_ex = (struct vl_queue_packet **)calloc(q->max_packets + plus, sizeof(struct vl_queue_packet *));
	if (packets_ex) {
		memcpy(packets_ex, q->packets, q->num_packets * sizeof(struct vl_queue_packet *));
		free(q->packets);
		q->max_packets += plus;
		q->packets = packets_ex;
	} else {
		VL_QUEUE_TRACE(q);
	}
}

int vl_queue_push(vl_queue_t q, const uint8_t *src, size_t size_byte) {
	struct vl_queue_packet *pkt;
	pkt = (struct vl_queue_packet *)malloc(sizeof(struct vl_queue_packet));
	if (pkt) {
		// create new packet
		pkt->data = (uint8_t *)malloc(size_byte * sizeof(uint8_t));
		VL_QUEUE_ASSERT(pkt->data);
		pkt->size = size_byte;
		memcpy(pkt->data, src, size_byte);
		// check queue size
		VL_QUEUE_ASSERT(q->num_packets <= q->max_packets);
		if  (q->num_packets == q->max_packets)  { // erase head packet
			struct vl_queue_packet *pkt_head = q->packets[0];
			q->num_packets--;
			memmove(q->packets, q->packets+1, q->num_packets * sizeof(struct vl_queue_packet *));
			vlq_free_packet(&pkt_head);
		}
		q->packets[q->num_packets] = pkt;
		q->num_packets++;
		if (q->num_packets >= q->max_packets && q->num_packets < VL_QUEUE_MAXIMUM_SIZE) 
			vlq_extend(q, q->max_packets);
		return size_byte;
	}
	else return -1;
}

int vl_queue_pop(vl_queue_t q, uint8_t *dst, size_t size_byte) {
	if (q->num_packets) {
		struct vl_queue_packet *pkt = q->packets[0];
		if (dst && size_byte) {
			int ret = pkt->size < size_byte? pkt->size: size_byte;
			q->num_packets--; 
			memmove(q->packets, q->packets+1, q->num_packets * sizeof(struct vl_queue_packet *));
			memcpy(dst, pkt->data, ret * sizeof(uint8_t));
			vlq_free_packet(&pkt);
			return ret;
		} else {
			return pkt->size;
		}
	} else {
		return 0;
	}
}

int vl_queue_pop_all(vl_queue_t q, uint8_t *dst, size_t size_byte) {
	int len = 0; 
	if (q->num_packets) {
		int num = q->num_packets;
		if (dst && size_byte) {
			size_t size_remain = size_byte;
			uint8_t *dst_curr = dst;
			int i;

			for (i = 0; i < num; i++) {
				struct vl_queue_packet *pkt = q->packets[i];
				if (size_remain >= pkt->size) {
					memcpy(dst_curr, pkt->data, pkt->size * sizeof(uint8_t));
					len += pkt->size;
					size_remain -= pkt->size;
					dst_curr += pkt->size;

					q->num_packets--;
					vlq_free_packet(&pkt);
				} else {
					memmove(q->packets, q->packets+i, q->num_packets * sizeof(struct vl_queue_packet *));
					break;
				}
			}
			VL_QUEUE_ASSERT(q->num_packets >= 0);
		} else {
			int i;
			for (i = 0; i < num; i++) {
				len += q->packets[i]->size;
			}
		}
	} 
	return len;
}

int vl_queue_get_size(const vl_queue_t q)
{
	return q->num_packets;
}
