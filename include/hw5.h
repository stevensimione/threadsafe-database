#include "cream.h"
#include "utils.h"
#include "csapp.h"
#include "queue.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <setjmp.h>

extern queue_t *fdque;
extern hashmap_t *self;

extern char* key[MAX_KEY_SIZE];
extern char* val[MAX_VALUE_SIZE];

extern struct request_header_t req;
extern struct response_header_t rep;

void help();

int stringToInt(char* num);

void *handle();

void exitHere();

void errorJump(int sig);

void destruct(map_key_t key, map_val_t val);