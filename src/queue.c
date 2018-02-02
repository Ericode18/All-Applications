#include "queue.h"
#include "errno.h"
#include "debug.h"
/*
 * This function will calloc(3) a new instance of queue_t and
 *    initilize all locks and semaphores in the queue_t struct.
 *
 *
 * @return A valid pointer to an initilized queue_t instance or NULL.
 *
 * Error Case: If calloc(3) returns NULL or if the locks fail to initilize, return NULL.
 */
queue_t *create_queue(void) {
    struct queue_t *queue = (struct queue_t*) calloc(1, sizeof(queue_t));
    if (queue == NULL) {
        return NULL;
    }
    int sem_return_code = sem_init(&((*queue).items), 0, 0);
    if (sem_return_code != 0) {
        return NULL;
    }

    int mutex = pthread_mutex_init(&queue->lock, NULL);

    if (mutex != 0) {
        return NULL;
    }

    return queue;
}

/*
 *  Free's the space allocated for the queue
 *
 *  @param self The pointer to the queue
 *  @param destroy_function will be called on all remaining items
 *                           in the queue and free the queue_node_t instances.
 *  @return true if the invalidation was successful
 *          false otherwise
 *
 *  @Error Case: if any parameters are invalid, set errno to EINVAL and return false
 */
bool invalidate_queue(queue_t *self, item_destructor_f destroy_function) {
    if(self == NULL || destroy_function == NULL) {
        errno = EINVAL;
        return false;
    }
    pthread_mutex_lock(&self->lock);
    queue_node_t *currentNode = (self->front);
    while(currentNode != NULL) {
        destroy_function(currentNode);
        free(currentNode);
        currentNode = currentNode->next;
    }
    self->invalid = true;
    self->front = NULL;
    self->rear = NULL;
    pthread_mutex_unlock(&self->lock);
    return true;
}

/*
 * This function will calloc(3) a new queue_node_t instance to add to the queue.
 *
 * @param self A pointer to the queue.
 * @param item A pointer to an item to add to the queue
 *
 * @return true if the operation was successful, false otherwise
 * Eror Case: If any parameters are inalid, set errno to EINVAL and return false.
 */
bool enqueue(queue_t *self, void *item) {
    if (item == NULL || self == NULL) {
        errno = EINVAL;
        return false;
    }
    pthread_mutex_lock(&self->lock);
    struct queue_node_t *qnode = (struct queue_node_t*) calloc(1, sizeof(queue_node_t));

    qnode->item = item;
    qnode->next = NULL;
    if(self->front == NULL) {
        self->front = qnode;
        self->rear = qnode;
    } else {
        self->rear->next = qnode;
        self->rear = qnode;
    }
    sem_post(&self->items);
    pthread_mutex_unlock(&self->lock);

    return true;
}

/*
 * Removes the item at the front of the queue pointed to by self.
 * This function will free(3) the queue_node_t instance being removed from the queue.
 * This function will block until an item is avalible to dequeue.
 *
 * @param self A pointer to the queue.
 *
 * @return A pointer to the item stored at the front of the queue.
 * Eror Case: If any parameters are inalid, set errno to EINVAL and return NULL.
 */
void *dequeue(queue_t *self) {
    if(self == NULL || self->invalid == 1) {
        errno = EINVAL;
        return false;
    }

    sem_wait(&self->items);
    pthread_mutex_lock(&self->lock);
    queue_node_t *frontNode = self->front;
    self->front = self->front->next;
    void *dequeueItem = frontNode->item;

    if(self->front == NULL) {
        self->rear = NULL;
    }
    free(frontNode);
    pthread_mutex_unlock(&self->lock);
    return dequeueItem;
}
