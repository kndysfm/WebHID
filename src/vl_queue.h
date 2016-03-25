/**
 *  Variable Length Queue module 
 *  It is only for queuing byte array
 */

#ifndef _VL_QUEUE_H_
#define _VL_QUEUE_H_

#include <stdint.h>

/**
 *  Type of the queue is pointer to struct
 */
struct _vl_queue;
typedef struct _vl_queue *vl_queue_t;

/**
 *  Create a new queue
 */
vl_queue_t vl_queue_create(void);

/**
 *  Release all byte arrays held in queue and resouce to manage the queue
 */
void vl_queue_destroy(vl_queue_t q);

/**
 *  Push a new byte array into bakc of queue
 *  It returns size_byte on success; -1 on error
 */
int vl_queue_push(vl_queue_t q, const uint8_t *src, size_t size_byte);

/**
 *  Pop the oldest byte array from front of queue
 *  It returns length of byte array; 0 on failed
 */
int vl_queue_pop(vl_queue_t q, uint8_t *dst, size_t size_byte);

/**
 *  Pop concatenated byte array fron front of queue
 *  It try to read byte arrays as far as buffer remains
 *  When dst or size_byte is zero, it returns the length of buffer
 *   to read all of arrays held in the queue
 */
int vl_queue_pop_all(vl_queue_t q, uint8_t *dst, size_t size_byte);

/**
 *  Returns number of byte-arrays buffered in Queue
 */
int vl_queue_get_size(const vl_queue_t q);

#endif //#ifndef _VL_QUEUE_H_