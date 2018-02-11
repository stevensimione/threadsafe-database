#include "queue.h"

queue_t *create_queue(void) {
    queue_t* init = calloc(1, sizeof(queue_t));

    if(init == NULL){
        free(init);
        return NULL;
    }

    pthread_mutex_t lockPtr;
    pthread_mutex_init(&lockPtr, NULL);
    sem_t itemsPtr;// = malloc(sizeof(sem_t));
    sem_init(&itemsPtr, 0, 0);
    init->lock = lockPtr;
    init->items = itemsPtr;
    init->invalid = false;

    return init;
}

bool invalidate_queue(queue_t *self, item_destructor_f destroy_function) {
    if(self == NULL || destroy_function == NULL || self->invalid == true){
        errno = EINVAL;
        return false;
    }
    /*int value = 0;
    sem_getvalue(&(self->items), &value);
*/

    pthread_mutex_lock(&self->lock);

    if(self->front != NULL){
        queue_node_t *node = self->front;

        if(self->front != self->rear){
            while(node != self->rear){
                queue_node_t *next = node->next;
                destroy_function(node->item);
                free(node);
                node = next;
            }
            destroy_function(node->item);
            free(node);

        }else{
            free(node);
        }
    }

    self->invalid = true;

    pthread_mutex_unlock(&self->lock);

    return true;
}

bool enqueue(queue_t *self, void *item) {
    if(self == NULL || item == NULL){
        errno = EINVAL;
        return false;
    }
    queue_node_t *enNode = calloc(1, sizeof(queue_node_t));

    pthread_mutex_lock(&self->lock);
    if(self->invalid == true){
        free(enNode);
        errno = EINVAL;
        pthread_mutex_unlock(&self->lock);
        return false;
    }

    enNode->item = item;
    if(self->rear != NULL){
        self->rear->next = enNode;
    }

    self->rear = enNode;
    if(self->front == NULL){
        self->front = enNode;
    }

    pthread_mutex_unlock(&self->lock);
    sem_post(&self->items);

    return true;
}

void *dequeue(queue_t *self) {
    if(self == NULL || self->invalid == true){
        errno = EINVAL;
        pthread_mutex_unlock(&self->lock);
        return false;
    }

    sem_wait(&self->items);

    pthread_mutex_lock(&self->lock);

    queue_node_t *deNode = self->front;
    if(self->front == self->rear){
        self->rear = NULL;
        self->front = NULL;
    }else{
        self->front = deNode->next;
    }
    void* item = deNode->item;
    free(deNode);

    pthread_mutex_unlock(&self->lock);

    return item;
}
