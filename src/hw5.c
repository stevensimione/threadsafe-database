#include "hw5.h"

queue_t *fdque;
hashmap_t *self;

char* key[MAX_KEY_SIZE];
char* val[MAX_VALUE_SIZE];

struct request_header_t req;
struct response_header_t rep;

jmp_buf buf;
int flag = 0;

void help(){
    printf("./cream [-h] NUM_WORKERS PORT_NUMBER MAX_ENTRIES\n" \
    "-h                 Displays this help menu and returns EXIT_SUCCESS.\n"\
    "NUM_WORKERS        The number of worker threads used to service requests.\n"\
    "PORT_NUMBER        Port number to listen on for incoming connections.\n"\
    "MAX_ENTRIES        The maximum number of entries that can be stored in `cream`'s underlying data store.\n"\
    "\n");

}

int stringToInt(char* num){
    int val = 0;
    int i = 0;
    while(*(num + i) != '\0'){
        val = val * 10;
        val = val + (*(num + i) - 48);
        i++;
    }
    return val;
}

void sendError(int connfd){
    rep.response_code = UNSUPPORTED;
    rep.value_size = 0;
    Rio_writen(connfd, &rep, sizeof(rep));
}

int handlePut(int connfd){

    errno = setjmp(buf);

    if((errno == EPIPE || errno == EINTR) && flag){
        goto putErr;
    }

    Rio_readn(connfd, &key, req.key_size);
    Rio_readn(connfd, &val, req.value_size);

    char *key_base = calloc(1, req.key_size);
    memcpy(key_base, key, req.key_size);
    char *val_base = calloc(1, req.value_size);
    memcpy(val_base, val, req.value_size);

    map_key_t map_key;
    map_val_t map_val;

    map_key.key_base = key_base;
    map_key.key_len = req.key_size;
    map_val.val_base = val_base;
    map_val.val_len = req.value_size;

    if(req.key_size < MIN_KEY_SIZE || req.key_size > MAX_KEY_SIZE ||
       req.value_size < MIN_VALUE_SIZE || req.value_size > MAX_VALUE_SIZE){
        rep.response_code = BAD_REQUEST;
        rep.value_size = 0;
    }else{
        if(put(self, map_key, map_val, false)){
            rep.response_code = OK;
            rep.value_size = req.value_size;
        }else{
            rep.response_code = BAD_REQUEST;
            rep.value_size = 0;
        }
    }


    Rio_writen(connfd, &rep, sizeof(rep));

    goto exitPut;

    putErr:
    free(key_base);
    free(val_base);
    flag = 0;

    exitPut:

    return 0;
}

int handleGet(int connfd){

    errno = setjmp(buf);

    if((errno == EPIPE || errno == EINTR) && flag){
        goto getErr;
    }

    Rio_readn(connfd, &key, req.key_size);

    void *key_base = calloc(1, req.key_size);
    memcpy(key_base, key, req.key_size);

    map_key_t map_key;

    map_key.key_base = key_base;
    map_key.key_len = req.key_size;

    if(req.key_size < MIN_KEY_SIZE || req.key_size > MAX_KEY_SIZE){
        rep.response_code = NOT_FOUND;
        rep.value_size = 0;
        Rio_writen(connfd, &rep, sizeof(rep));
    }else{
        map_val_t map_val = get(self, map_key);
        if(map_val.val_base != NULL){
            rep.response_code = OK;
            rep.value_size = map_val.val_len;
            Rio_writen(connfd, &rep, sizeof(rep));
            Rio_writen(connfd, map_val.val_base, map_val.val_len);
        }else{
            rep.response_code = NOT_FOUND;
            rep.value_size = 0;
            Rio_writen(connfd, &rep, sizeof(rep));
        }
    }

    free(key_base);

    goto exitGet;

    getErr:

    free(key_base);
    flag = 0;

    exitGet:

    return 0;
}

int handleClear(int connfd){
    errno = setjmp(buf);

    if((errno == EPIPE || errno == EINTR) && flag){
        goto clearErr;
    }

    clear_map(self);

    rep.response_code = OK;
    rep.value_size = 0;
    Rio_writen(connfd, &rep, sizeof(rep));

    clearErr:

    flag = 0;

    return 0;
}

int handleEvict(int connfd){

    errno = setjmp(buf);

    if((errno == EPIPE || errno == EINTR) && flag){
        goto evictErr;
    }

    Rio_readn(connfd, &key, req.key_size);

    char *key_base = calloc(1, req.key_size);
    memcpy(key_base, key, req.key_size);

    map_key_t map_key;

    map_key.key_base = key_base;
    map_key.key_len = req.key_size;

    if(!(req.key_size < MIN_KEY_SIZE || req.key_size > MAX_KEY_SIZE)){
        delete(self, map_key);
    }


    rep.response_code = OK;
    rep.value_size = 0;
    Rio_writen(connfd, &rep, sizeof(rep));

    free(key_base);

    goto evictExit;

    evictErr:

    free(key_base);
    flag = 0;

    evictExit:

    return 0;
}

void queue_free_function2(void *item) {
    free(item);
}

void exitHere(){
    invalidate_map(self);
    invalidate_queue(fdque, queue_free_function2);
    sem_destroy(&(fdque->items));
    pthread_mutex_destroy(&(fdque->lock));
    pthread_mutex_destroy(&(self->write_lock));
    pthread_mutex_destroy(&(self->fields_lock));
    free(self);
    free(fdque);
    exit(0);
}

void errorJump(int sig){
    flag = 1;
    longjmp(buf, errno);
}

void *handle(){
    while(1){
    int *fd = dequeue(fdque);
    int connfd = *fd;

    Rio_readn(connfd, &req, sizeof(req));
    if(req.request_code == PUT){
        handlePut(connfd);
    }else if(req.request_code == GET){
        handleGet(connfd);
    }else if(req.request_code == EVICT){
        handleEvict(connfd);
    }else if(req.request_code == CLEAR){
        handleClear(connfd);
    }else{
        sendError(connfd);
    }
    fflush(NULL);
    close(connfd);
    }
}

void destruct(map_key_t key, map_val_t val){
    free(key.key_base);
    free(val.val_base);
}
