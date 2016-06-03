#include "cache.h"
#include<stdio.h>
#include<stdlib.h>

int mem[1000000];

int main(int argc, char* argv[]){

    int msg_id = shmget(MSG_NAME(0), 0, 0);
    volatile int* msg0 = shmat(msg_id, 0, 0);
    msg_id = shmget(MSG_NAME(1), 0, 0);
    volatile int* msg1 = shmat(msg_id, 0, 0);
    read_request(msg0, 6, 1);
    read_request(msg1, 3, 1);
    read_request(msg0, 8, 1);
    read_request(msg1, 9, 1);
    write_request(msg0, 1, 1, 1);
    write_request(msg0, 4, 3, 1);
    write_request(msg0, 2, 4, 1);
    write_request(msg1, 8, 3, 1);
    write_request(msg1, 7, 0, 1);
    int ans = read_request(msg0, 8, 1);
    printf("%d\n", ans);
    return 0;
}
