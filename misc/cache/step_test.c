#include "cache.h"
#include<stdio.h>
#include<stdlib.h>

int mem[1000000];

int main(int argc, char* argv[]){

    int msg_id = shmget(MSG_NAME(0), 0, 0);
    volatile int* msg0 = shmat(msg_id, 0, 0);
    msg_id = shmget(MSG_NAME(1), 0, 0);
    volatile int* msg1 = shmat(msg_id, 0, 0);
    write_request(msg0, 1, 8);
    write_request(msg1, 1, 8);
    read_request(msg1, 1);
    read_request(msg0, 1);
    return 0;
}
