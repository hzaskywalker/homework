#include "cache.h"
#include<stdio.h>
#include<stdlib.h>

int main(int argc, char* argv[]){
    int id = 0;
    if(argc==1)
        id = 0;
    else
        id = atoi(argv[1]);

    int mem_id = shmget(MEM_NAME, 0, 0);
    int bus_id = shmget(BUS_NAME, 0, 0);
    volatile byte_t* mem = shmat(mem_id, 0, 0);
    volatile int* bus = shmat(bus_id, 0, 0);
    if(mem == (byte_t*)-1 || bus == (int*)-1)
        printf("error when want get shared_memory\nmem: %p, bus: %p\n", mem, bus);
    else{
        printf("id: %d, mem: %p, bus: %p\n", id, mem, bus);
    }

    cache_t c = init_cache(MAX_CACHE_LEN, id, mem, bus);

    int msg_id = shmget(MSG_NAME(id), 16* sizeof(int), IPC_CREAT|0666);
    volatile int* msg = shmat(msg_id, 0, 0);
    if(msg == (int*) -1){
        printf("msg initialize failed: %p\n", msg);
        exit(0);
    }
    msg[0]=0;
    printf("cache %d ready, msg_id: %d\n", id, msg_id);
    while(1){
        answer(c);
        if(msg[0]){
            if(msg[0]==1){
                //Read
                int pos = msg[1];
#ifdef DEBUG
//                printf("%d READ: %d\n", id, pos);
#endif
                int ans = ONE_STEP(c, pos, 0, READ, msg[4]);
                msg[3] = ans;

                msg[0] = 0;
            }
            else if(msg[0] ==2){
                int pos = msg[1];
                int val = msg[2];
                ONE_STEP(c, pos, val, WRITE, msg[4]);
                msg[0] = 0;
            }
            else if(msg[0] ==3){
                //TEST, return the number in the mem
                //if zero, set to 1
                //otherwise no change
                //element opr
                int pos = msg[1];
                msg[3] = ONE_STEP(c, pos, 0, TEST, msg[4]);
                msg[0] = 0;
            }
        }
    }
    return 0;
}
