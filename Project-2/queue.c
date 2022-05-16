#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define TRUE  1
#define FALSE 0

typedef struct {
    int ID;
    char type;
    time_t requestTime;
    time_t endTime;
    time_t turnAroundTime;
    char pad;
    int size;
} Job;

/* a link in the queue, holds the data and point to the next Node */
typedef struct Node_t {
    Job data;
    struct Node_t *prev;
} NODE;

/* the HEAD of the Queue, hold the amount of node's that are in the queue */
typedef struct Queue {
    NODE *head;
    NODE *tail;
    int size;
    int limit;
} Queue;

Queue *ConstructQueue(int limit);
void DestructQueue(Queue *queue);
int Enqueue(Queue *pQueue, Job j);
Job Dequeue(Queue *pQueue);
int isEmpty(Queue* pQueue);
void printQueueId(Queue* pQueue);
Job front(Queue *pQueue);
int EnqueueToFront(Queue *pQueue, Job j);

Queue *ConstructQueue(int limit) {
    Queue *queue = (Queue*) malloc(sizeof (Queue));
    if (queue == NULL) {
        return NULL;
    }
    if (limit <= 0) {
        limit = 65535;
    }
    queue->limit = limit;
    queue->size = 0;
    queue->head = NULL;
    queue->tail = NULL;

    return queue;
}

void DestructQueue(Queue *queue) {
    NODE *pN;
    while (!isEmpty(queue)) {
        Dequeue(queue);
    }
    free(queue);
}

int Enqueue(Queue *pQueue, Job j) {
    /* Bad parameter */
    NODE* item = (NODE*) malloc(sizeof (NODE));
    item->data = j;

    if ((pQueue == NULL) || (item == NULL)) {
        return FALSE;
    }
    // if(pQueue->limit != 0)
    if (pQueue->size >= pQueue->limit) {
        return FALSE;
    }
    /*the queue is empty*/
    item->prev = NULL;
    if (pQueue->size == 0) {
        pQueue->head = item;
        pQueue->tail = item;

    } else {
        /*adding item to the end of the queue*/
        pQueue->tail->prev = item;
        pQueue->tail = item;
    }
    pQueue->size++;
    return TRUE;
}
//Adds a job to the front of the queue
int EnqueueToFront(Queue *pQueue, Job j) {
    /* Bad parameter */
    NODE* item = (NODE*) malloc(sizeof (NODE));
    item->data = j;

    if ((pQueue == NULL) || (item == NULL)) {
        return FALSE;
    }
    // if(pQueue->limit != 0)
    if (pQueue->size >= pQueue->limit) {
        return FALSE;
    }
    /*the queue is empty*/
    item->prev = NULL;
    if (pQueue->size == 0) {
        pQueue->head = item;
        pQueue->tail = item;

    } else {
        /*adding item to the end of the queue*/
        item->prev = pQueue->head;
        pQueue->head = item;
    }
    pQueue->size++;
    return TRUE;
}
Job Dequeue(Queue *pQueue) {
    /*the queue is empty or bad param*/
    NODE *item;
    Job ret;
    if (isEmpty(pQueue))
        return ret;
    item = pQueue->head;
    pQueue->head = (pQueue->head)->prev;
    pQueue->size--;
    ret = item->data;
    free(item);
    return ret;
}

int isEmpty(Queue* pQueue) {
    if (pQueue == NULL) {
        return FALSE;
    }
    if (pQueue->size == 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}
//Prints the IDs of job in the queue
void printQueueId(Queue* pQueue) {
    NODE *item;
    Job j;

    Queue *temp = (Queue*) malloc(sizeof (Queue));
    temp->head = pQueue->head;
    temp->size = pQueue->size;
    temp->limit = pQueue->limit;
    temp->tail = pQueue->tail;
    
    while (temp->size > 0) {
        item = temp->head;
        temp->head = (temp->head)->prev;
        temp->size--;
        j = item->data;
        printf("%d, ", j.ID);
    }
    printf("\n"); 
    free(temp);
}
//Gives the job in front of the queue
Job front(Queue *pQueue) {
    NODE *item;
    Job ret;
    if (isEmpty(pQueue))
        return ret;
    item = pQueue->head;
    ret = item->data;
    return ret;
}
