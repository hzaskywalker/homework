#include<sys/shm.h>
#include<sys/types.h>
#include<errno.h>
#include<stdio.h>
#include<sys/ipc.h> 
#include<stdlib.h>
#include"shareinfo.h"
#include<semaphore.h>
#define PERM S_IRUSR|S_IWUSR 
typedef unsigned char byte_t;

int main(){
    printf("%d %d\n", MEM_NAME, BUS_NAME);
    volatile int mem_id = shmget(MEM_NAME, MAX_MEM_POS * sizeof(byte_t), IPC_CREAT|0666);
    volatile int bus_id = shmget(BUS_NAME, MAX_BUS_LEN * sizeof(int), IPC_CREAT|0666);
    byte_t* mem = shmat(mem_id, 0, 0);
    int* bus = shmat(bus_id, 0, 0);
    if(mem==(byte_t*)-1 || bus == (int*) -1){
        printf("shmat failed mem: %p, bus: %p\n", mem, bus);
        exit(0);
    }
    else{
        printf("mem: %p, bus: %p\n", mem, bus);
    }
    int i;
    for(i=0;i<MAX_MEM_POS;++i){
        mem[i]=0;
    }
    for(i=0;i<MAX_BUS_LEN;++i){
        bus[i]=0;
    }
    while(1){
    }
    return 0;
}
