/*
 * The programming concept of this code was mainly impressed by 
 *   Stephen Prata, C Primer Plus, Fifth Edition
 */

/* queue.h -- Interface for a queue, based on abstract data types
 *            stored in a linked list. */

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdbool.h>

/*maximum item count in queue*/
#define MAXQUEUE 100

typedef struct item {
    char* message;
} qitem_t;

typedef struct node {
    qitem_t item;
    struct node* next;
} qnode_t;

typedef struct queue {
    qnode_t* front;     /* pointer to front of queue */
    qnode_t* rear;      /* pointer to rear of queue  */
    unsigned int items; /* number of items in queue  */
} queue_t;

/* Initialize the queue; pq points to a queue which is initialized
 * to being empty */
void initialize_queue(queue_t* pq);

/* Check if queue is full; pq points to previously initialized queue. 
 * Returns true if queue is full, else false  */
bool queue_is_full(const queue_t* pq);

/* Check if queue is empty; pq points to previously initialized queue.
 * Returns true if queue is empty, else false. */
bool queue_is_empty(const queue_t* pq);

/* Determine number of items in queue; pq points to previously
 * initialized queue. Returns number of items in queue */
unsigned int queue_item_count(const queue_t* pq);

/* Add item to rear of queue; pq points to previously initialized
 * queue. Item is to be placed at rear of queue. If queue is not
 * empty, item is placed at rear of queue and function returns true;
 * otherwise, queue is unchanged and function returns false */
bool enqueue(qitem_t item, queue_t* pq);

/* Remove item from front of queue; pq points to previously initialized
 * queue. If queue is not empty, item at head of queue is copied to
 * pitem and deleted from queue, and function returns true; if the  
 * operation empties the queue, the queue is reset to empty. If the
 * queue is empty to begin with, queue is unchanged and the function
 * returns false. */
bool dequeue(qitem_t* pitem, queue_t* pq);

/* Empty the queue; pq points to previously initialized queue the queue
 * is empty. */
void clear_queue(queue_t* pq);

#endif
