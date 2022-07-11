#include "queue.h"

void enqueue(queue_t *queue, void *val) {
    node_t *new_node = malloc(sizeof(node_t));
    if (!new_node) return;

    new_node->val = val;
    new_node->next = queue->head;
    new_node->prev = 0;
    if(queue->head)
        queue->head->prev = new_node;
    else
        queue->tail = new_node;

    queue->head = new_node;
}

void* dequeue(queue_t *queue) {
    node_t *tail = queue->tail;
    void* retval;

    if (tail == 0) return 0;
    else              retval = tail->val;

    queue->tail = tail->prev;
    free(tail);

    if (queue->tail == 0)
        queue->head = 0;
    else
        queue->tail->next = 0;

    return retval;
}

void push(queue_t *queue, void *val){
    if(queue->tail){
        queue->tail->next = malloc(sizeof(node_t));
        queue->tail->next->prev = queue->tail;
        queue->tail = queue->tail->next;
    }
    else{
        queue->tail = malloc(sizeof(node_t));
        queue->tail->prev = 0;
        queue->head = queue->tail;
    }
    queue->tail->next = 0;
    queue->tail->val = val;
}
