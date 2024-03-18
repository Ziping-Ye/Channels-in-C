#include "channel.h"

// This program implements the channel_create, channel_destroy, channel_close, channel_send, channel_receive, and channel_select functions

// Creates a new channel with the provided size and returns it to the caller
// A 0 size indicates an unbuffered channel, whereas a positive size indicates a buffered channel
chan_t* channel_create(size_t size)
{
    if (size < 0)
        return NULL;

    chan_t* channel = (chan_t *) malloc(sizeof(chan_t));
    // Initialize all fields of the chan_t struct
    channel->buffer = buffer_create(size);
    channel->closed = 0;
    pthread_mutex_init(&channel->bmutex, NULL);
    pthread_mutex_init(&channel->lmutex, NULL);
    sem_init(&channel->sender, 0, (unsigned int) size);
    sem_init(&channel->receiver, 0, 0);
    channel->send_list = list_create();
    channel->receive_list = list_create();
    return channel;
}

// Writes data to the given channel
// This can be both a blocking call i.e., the function only returns on a successful completion of send (blocking = true), and
// a non-blocking call i.e., the function simply returns if the channel is full (blocking = false)
// In case of the blocking call when the channel is full, the function waits till the channel has space to write the new data
// Returns SUCCESS for successfully writing data to the channel,
// WOULDBLOCK if the channel is full and the data was not added to the buffer (non-blocking calls only),
// CLOSED_ERROR if the channel is closed, and
// OTHER_ERROR on encountering any other generic error of any sort
enum chan_status channel_send(chan_t* channel, void* data, bool blocking)
{
    //blocking call
    if (blocking) {
        sem_wait(&channel->sender);
        pthread_mutex_lock(&channel->bmutex);
        if (channel->closed) { //if channel is closed, return CLOSED_ERROR
            pthread_mutex_unlock(&channel->bmutex);
            sem_post(&channel->sender); //poke another sender (to achieve "broadcast")
            return CLOSED_ERROR;
        }
        buffer_add(data, channel->buffer);
        pthread_mutex_unlock(&channel->bmutex);
        sem_post(&channel->receiver); //notify the receiver that there is one element to consume if receiver is blocking
        //notify all selects that subscribe to receive operation on this channel
        pthread_mutex_lock(&channel->lmutex);
        list_node_t* p = channel->receive_list->head;
        while (p != NULL) {
            sem_post((sem_t *) (p->data));
            p = p->next;
        }
        pthread_mutex_unlock(&channel->lmutex);
        return SUCCESS;
    } else { //non-blocking call
        if (sem_trywait(&channel->sender) == 0) { //success - same to sem_wait()
            pthread_mutex_lock(&channel->bmutex);
            if (channel->closed) {
                pthread_mutex_unlock(&channel->bmutex);
                sem_post(&channel->sender);
                return CLOSED_ERROR;
            }
            buffer_add(data, channel->buffer);
            pthread_mutex_unlock(&channel->bmutex);
            sem_post(&channel->receiver);

            pthread_mutex_lock(&channel->lmutex);
            list_node_t* p = channel->receive_list->head;
            while (p != NULL) {
                sem_post((sem_t *) (p->data));
                p = p->next;
            }
            pthread_mutex_unlock(&channel->lmutex);

            return SUCCESS;
        } else { //unsuccess - won't block, just return
            //check if channel is closed - return CLOSED_ERROR
            pthread_mutex_lock(&(channel->bmutex));
            if (channel->closed) {
                pthread_mutex_unlock(&channel->bmutex);
                return CLOSED_ERROR;
            }
            //not closed - return WOULDBLOCK
            pthread_mutex_unlock(&channel->bmutex);
            return WOULDBLOCK;
        }
    }
}

// Reads data from the given channel and stores it in the function’s input parameter, data (Note that it is a double pointer).
// This can be both a blocking call i.e., the function only returns on a successful completion of receive (blocking = true), and
// a non-blocking call i.e., the function simply returns if the channel is empty (blocking = false)
// In case of the blocking call when the channel is empty, the function waits till the channel has some data to read
// Returns SUCCESS for successful retrieval of data,
// WOULDBLOCK if the channel is empty and nothing was stored in data (non-blocking calls only),
// CLOSED_ERROR if the channel is closed, and
// OTHER_ERROR on encountering any other generic error of any sort
enum chan_status channel_receive(chan_t* channel, void** data, bool blocking)
{
    //blocking call
    if (blocking) {
        sem_wait(&channel->receiver);
        pthread_mutex_lock(&channel->bmutex);
        if (channel->closed) {
            pthread_mutex_unlock(&channel->bmutex);
            sem_post(&channel->receiver);
            return CLOSED_ERROR;
        }
        *data = buffer_remove(channel->buffer);
        pthread_mutex_unlock(&channel->bmutex);
        sem_post(&channel->sender); //notify the sender that there is one available spot in the buffer to send if sender is blocking
        //notify all selects that subscribe to send operation on this channel
        pthread_mutex_lock(&channel->lmutex);
        list_node_t* p = channel->send_list->head;
        while (p != NULL) {
            sem_post((sem_t *) (p->data));
            p = p->next;
        }
        pthread_mutex_unlock(&channel->lmutex);
        return SUCCESS;
    } else { //non-blocking call
        if (sem_trywait(&channel->receiver) == 0) { //success - same to sem_wait()
            pthread_mutex_lock(&channel->bmutex);
            if (channel->closed) {
                pthread_mutex_unlock(&channel->bmutex);
                sem_post(&channel->receiver);
                return CLOSED_ERROR;
            }
            *data = buffer_remove(channel->buffer);
            pthread_mutex_unlock(&channel->bmutex);
            sem_post(&channel->sender);

            pthread_mutex_lock(&channel->lmutex);
            list_node_t* p = channel->send_list->head;
            while (p != NULL) {
                sem_post((sem_t *) (p->data));
                p = p->next;
            }
            pthread_mutex_unlock(&channel->lmutex);
            return SUCCESS;
        } else { //unsuccess
            //check if channel is closed - return CLOSED_ERROR
            pthread_mutex_lock(&channel->bmutex);
            if (channel->closed) {
                pthread_mutex_unlock(&channel->bmutex);
                return CLOSED_ERROR;
            }
            //not closed - return WOULDBLOCK
            pthread_mutex_unlock(&channel->bmutex);
            return WOULDBLOCK;
        }
    }
    return OTHER_ERROR;
}

// Closes the channel and informs all the blocking send/receive/select calls to return with CLOSED_ERROR
// Once the channel is closed, send/receive/select operations will cease to function and just return CLOSED_ERROR
// Returns SUCCESS if close is successful,
// CLOSED_ERROR if the channel is already closed, and
// OTHER_ERROR in any other error case
enum chan_status channel_close(chan_t* channel)
{
    pthread_mutex_lock(&channel->bmutex);
    int closed = channel->closed;
    channel->closed = 1;
    pthread_mutex_unlock(&channel->bmutex);
    if (closed) { //if channel is already closed, return CLOSED_ERROR
        return CLOSED_ERROR;
    }
    sem_post(&channel->sender); //poke one sender (to achieve "broadcast")
    //notify all selects that subscribe send operation on this channel
    pthread_mutex_lock(&channel->lmutex);
    list_node_t* p = channel->send_list->head;
    while (p != NULL) {
        sem_post((sem_t *) (p->data));
        p = p->next;
    }
    pthread_mutex_unlock(&channel->lmutex);

    sem_post(&channel->receiver); //poke one receiver (to achieve "broadcast")
    //notify all selects that subscribe receive operation on this channel
    pthread_mutex_lock(&channel->lmutex);
    list_node_t* q = channel->receive_list->head;
    while (q != NULL) {
        sem_post((sem_t *) (q->data));
        q = q->next;
    }
    pthread_mutex_unlock(&channel->lmutex);
    return SUCCESS;
}

// Frees all the memory allocated to the channel
// The caller is responsible for calling channel_close and waiting for all threads to finish their tasks before calling channel_destroy
// Returns SUCCESS if destroy is successful,
// DESTROY_ERROR if channel_destroy is called on an open channel, and
// OTHER_ERROR in any other error case
enum chan_status channel_destroy(chan_t* channel)
{
    pthread_mutex_lock(&channel->bmutex);
    if (channel->closed == 0) { //if channel is not closed yet
        pthread_mutex_unlock((&channel->bmutex));
        return DESTROY_ERROR;
    }
    pthread_mutex_unlock(&channel->bmutex);
    // Deallocate all resources related to this channel
    buffer_free(channel->buffer);
    pthread_mutex_destroy(&channel->bmutex);
    pthread_mutex_destroy(&channel->lmutex);
    sem_destroy(&channel->sender);
    sem_destroy(&channel->receiver);
    list_destroy(channel->send_list);
    list_destroy(channel->receive_list);
    free(channel);
    return SUCCESS;
}

// Takes an array of channels, channel_list, of type select_t and the array length, channel_count, as inputs
// This API iterates over the provided list and finds the set of possible channels which can be used to invoke the required operation (send or receive) specified in select_t
// If multiple options are available, it selects the first option and performs its corresponding action
// If no channel is available, the call is blocked and waits till it finds a channel which supports its required operation
// Once an operation has been successfully performed, select should set selected_index to the index of the channel that performed the operation and then return SUCCESS
// In the event that a channel is closed or encounters any error, the error should be propagated and returned through select
// Additionally, selected_index is set to the index of the channel that generated the error
enum chan_status channel_select(size_t channel_count, select_t* channel_list, size_t* selected_index)
{
    //local variable - specific to each instance of select call
    sem_t selecter;
    sem_init(&selecter, 0, 0);

    //subscribe to all channels in the channel_list
    for (int i = 0; i < channel_count; i++) {
        if (channel_list[i].is_send) { //send operation
            pthread_mutex_lock(&(channel_list[i].channel->lmutex));
            list_insert(channel_list[i].channel->send_list, &selecter);
            pthread_mutex_unlock(&(channel_list[i].channel->lmutex));
        } else { //receive operation
            pthread_mutex_lock(&(channel_list[i].channel->lmutex));
            list_insert(channel_list[i].channel->receive_list, &selecter);
            pthread_mutex_unlock(&(channel_list[i].channel->lmutex));
        }
    }

    enum chan_status result;
    //try to perform one operation
    while (true) {
        //try each operation in channel_list using non-blocking manner
        for (size_t i = 0; i < channel_count; i++) {
            if (channel_list[i].is_send) { //send operation
                result = channel_send(channel_list[i].channel, channel_list[i].data, false);
            } else { //receive operation
                result = channel_receive(channel_list[i].channel, &(channel_list[i].data), false);
            }
            if (result == SUCCESS || result == CLOSED_ERROR || result == OTHER_ERROR) { //we can return
                *selected_index = i;

                //unsubscribe before return
                for (int j = 0; j < channel_count; j++) {
                    if (channel_list[j].is_send) { //send operation
                        pthread_mutex_lock(&(channel_list[j].channel->lmutex));
                        list_remove(channel_list[j].channel->send_list, list_find(channel_list[j].channel->send_list, &selecter));
                        pthread_mutex_unlock(&(channel_list[j].channel->lmutex));
                    } else { //receive operation
                        pthread_mutex_lock(&(channel_list[j].channel->lmutex));
                        list_remove(channel_list[j].channel->receive_list, list_find(channel_list[j].channel->receive_list, &selecter));
                        pthread_mutex_unlock(&(channel_list[j].channel->lmutex));
                    }
                }
                sem_destroy(&selecter);
                return result;
            } else { //result == WOULDBLOCK
                continue; //try next operation
            }
        }
        //already iterate over all operations in channel, but made no progress, sem_wait
        sem_wait(&selecter);
    }
}
