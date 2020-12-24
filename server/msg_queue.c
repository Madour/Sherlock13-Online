#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server/msg_queue.h"

void MsgQueue_Init(MsgQueue* queue) {
    queue->size = 0;
    queue->head = NULL;
    queue->tail = NULL;
}

MsgNode* MsgQueue_Front(MsgQueue* queue) {
    return queue->head;
}

void MsgQueue_Append(MsgQueue* queue, char* msg, int size, int dest) {
    MsgNode* new_node = malloc(sizeof(MsgNode));

    strcpy(new_node->msg, msg);
    new_node->size = size;
    new_node->dest = dest;
    new_node->next = NULL;

    if (queue->size == 0)
        queue->head = new_node;
    else
        queue->tail->next = new_node;
    queue->tail = new_node;

    queue->size++;
}

void MsgQueue_Pop(MsgQueue* queue) {
    if (queue->size == 0)
        return;

    MsgNode* old_head = queue->head;
    queue->head = queue->head->next;
    free(old_head);

    queue->size--;
}

void MsgQueue_Clear(MsgQueue* queue) {
    while (queue->size > 0) {
        MsgQueue_Pop(queue);
    }
    MsgQueue_Init(queue);
}

void MsgQueue_Print(MsgQueue* queue) {
    MsgNode* node;
    int i = 0;
    for (node = queue->head; node != NULL; node = node->next)
        printf("%d - msg: %s  size: %d  dest: %d\n", i++, node->msg, node->size, node->dest);
}
