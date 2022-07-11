
typedef struct node {
    void *val;
    struct node *next;
    struct node *prev;
} node_t;

typedef struct queue{
    node_t *head;
    node_t *tail;
} queue_t;

void enqueue(queue_t *, void *);
void* dequeue(queue_t *);
void push(queue_t *, void *);
