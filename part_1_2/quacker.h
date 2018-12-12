#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#define NUM_TOPICS 1
#define MAX_ENTRIES 8
#define QUACK_SIZE 140


struct topicentry{
    int entrynum;
    struct timeval timestamp;
    int pubID;
    int message;
};

struct queue{
    pthread_mutex_t topic_lock;
    struct topicentry circular_buffer[MAX_ENTRIES];
    int in;
    int out;
    int topic_counter;
};

struct pub_process{
    pid_t pid;
    // pthread_t pub_tid;
    int pub_to_quacker[2];
    int quacker_to_pub[2];
};

struct sub_process{
    pid_t pid;
    // pthread_t sub_tid;
    int sub_to_quacker[2];
    int quacker_to_sub[2];
};
