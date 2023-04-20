#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <stdint.h>

#define QUEUE_INIT_LEN 16

typedef void (*queue_constuctor_t)(void *dst, const void *src, size_t len);
// FIFO 循环队列
typedef struct queue
{
    void **data; // 数据
    queue_constuctor_t value_constuctor;
    size_t head;   // 队列头
    size_t tail;   // 队列尾
    size_t item_size;
} queue_t;

queue_t* queue_init(size_t item_size, queue_constuctor_t value_constuctor);
int queue_append(queue_t *queue, void* item);
int queue_get(queue_t *queue, void* dst);
int queue_peek(queue_t *queue, void* dst);
int queue_empty(queue_t * queue);
int queue_destroy(queue_t *queue);
#endif