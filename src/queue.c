#include "queue.h"
#include <stdlib.h>
#include <string.h>

/// @brief 指针队列，FIFO，别忘了destroy
/// @return queue 队列指针
queue_t* queue_init(size_t item_size, queue_constuctor_t value_constuctor)
{
    queue_t* queue = (queue_t*)malloc(sizeof(queue_t));
    queue->data = (void**)malloc(QUEUE_INIT_LEN*item_size);
    queue->head = 0;
    queue->tail = 0;
    queue->item_size = item_size;
    if (value_constuctor == NULL)
    {
        value_constuctor = (queue_constuctor_t)memcpy;
    }
    queue->value_constuctor = value_constuctor;
    return queue;
}
int queue_append(queue_t *queue, void* item)
{
    if (((queue->tail + 1) % QUEUE_INIT_LEN) == queue->head) return -1;
    void * item_loc = queue->data + queue->item_size * queue->tail;
    queue->value_constuctor(item_loc, item, queue->item_size);
    queue->tail = (queue->tail + 1) % QUEUE_INIT_LEN;
    return 0;
}

int queue_get(queue_t *queue, void* dst)
{
    if (queue_peek(queue,dst) != 0) return -1;
    queue->head = (queue->head + 1) % QUEUE_INIT_LEN;
    return 0;
}
int queue_peek(queue_t *queue, void* dst)
{
    if (queue->head == queue->tail) return -1;
    void* item = queue->data + queue->item_size*queue->head;
    queue->value_constuctor(dst, item, queue->item_size);
    return 0;
}
int queue_empty(queue_t * queue)
{
    return (queue->head == queue->tail) ? 1 : 0;
}
int queue_destroy(queue_t *queue)
{
    free(queue->data);
    free(queue);
    return 0;
}
