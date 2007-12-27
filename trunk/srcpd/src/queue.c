/* queue.c -- double linked list queue implementation*/
#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

/* local functions */
static void copy_to_node(qitem_t item, qnode_t* pn)
{
    pn->item = item;
}

static void copy_to_item(qnode_t* pn, qitem_t* pi)
{
    *pi = pn->item;
}

void initialize_queue(queue_t* pq)
{
    pq->front = pq->rear = NULL;
    pq->items = 0;
}

bool queue_is_full(const queue_t* pq)
{
    return pq->items == MAXQUEUE;
}

bool queue_is_empty(const queue_t* pq)
{
    return pq->items == 0;
}

unsigned int queue_item_count(const queue_t* pq)
{
    return pq->items;
}

bool enqueue(qitem_t item, queue_t* pq)
{
    qnode_t* pnew;

    if (queue_is_full(pq))
        return false;

    pnew = (qnode_t*) malloc(sizeof(qnode_t));
    if (pnew == NULL)
    {
        /*
        fprintf(stderr,"Unable to allocate memory!\n");
        exit(1);
        */
        return false;
    }
    copy_to_node(item, pnew);
    pnew->next = NULL;
    if (queue_is_empty(pq))
        pq->front = pnew;           /* item goes to front     */
    else
        pq->rear->next = pnew;      /* link at end of queue   */
    pq->rear = pnew;                /* record location of end */
    pq->items++;                    /* one more item in queue */
   
    return true;
}

bool dequeue(qitem_t* pitem, queue_t* pq)
{
    qnode_t* pt;

    if (queue_is_empty(pq))
        return false;

    copy_to_item(pq->front, pitem);
    pt = pq->front;
    pq->front = pq->front->next;
    free(pt);
    pq->items--;
    if (pq->items == 0)
        pq->rear = NULL;
 
    return true;
}

/* empty the queue */
void clear_queue(queue_t* pq)
{
    qitem_t dummy;
    while (!queue_is_empty(pq))
        dequeue(&dummy, pq);
}

