#include "hw5.h"

int main(int argc, char *argv[]) {
    if(argc <= 2){
        help();
    if(argv[1] == NULL){
        return EXIT_SUCCESS;
    }else{
        return EXIT_FAILURE;
    }
    }

    if(argc != 4){
        if(strcmp("-h", argv[1]) == 0){
            help();
            return EXIT_SUCCESS;
        }else{
            return EXIT_FAILURE;
        }
    }

    signal(SIGPIPE, errorJump);
    signal(SIGINT, exitHere);

    int NUM_WORKERS, PORT_NUMBER, MAX_ENTRIES;
    socklen_t clientlen;

    NUM_WORKERS = stringToInt(argv[1]);
    PORT_NUMBER = stringToInt(argv[2]);
    MAX_ENTRIES = stringToInt(argv[3]);

    fdque = create_queue();
    self = create_map(MAX_ENTRIES, jenkins_one_at_a_time_hash, destruct);
    pthread_t td[NUM_WORKERS];
    for(int i = 0; i < NUM_WORKERS; i++){
        if(pthread_create(&td[i], NULL, handle, NULL) != 0){
            return EXIT_FAILURE;
        }
    }

    int listenfd;
    struct sockaddr_in clientaddr;

    //struct hostent *hp;
    //char* haddrp;
    NUM_WORKERS = NUM_WORKERS;
    MAX_ENTRIES = MAX_ENTRIES;

    listenfd = open_listenfd(PORT_NUMBER);
    int connfd;
    while(1){
        clientlen = sizeof(clientaddr);
        connfd = accept(listenfd, (struct sockaddr*) &clientaddr, &clientlen);
        enqueue(fdque, &connfd);
    }
    exit(0);
}
