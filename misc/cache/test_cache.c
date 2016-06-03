#include "cache.h"
#include<stdio.h>
#include<stdlib.h>

int mem[1000000];

int main(int argc, char* argv[]){
    //Notice that time may be the same for two cache
    int id = 0;
    if(argc==1)
        id = 0;
    else
        id = atoi(argv[1]);
    srand(time(0)+id);
    printf("cache id: %d\n", id);
    fflush(stdout);

    int msg_id = shmget(MSG_NAME(id), 0, 0);
    volatile int* msg = shmat(msg_id, 0, 0);
    if(msg == (int*) -1){
        printf("get msg failed: %p\n", msg);
        exit(0);
    }
    int i;
#ifdef DEBUG
    FILE* tmpf = fopen("test_example", "w");
#endif

    char filename[] = "ans_0.txt";
    if(id==1)filename[4] ='1';
    FILE* result = fopen(filename, "w");
    for(i=0;i<10000;++i){
        int pos = rand() % 20000, val=rand() % 256, type = rand()%2;
#ifdef DEBUG
        fprintf(tmpf, "%d %d %d\n", type, pos, val);
        fflush(tmpf);
#endif
        if(type==0){
            int readed_val = read_request(msg, pos, 1);
#ifdef DEBUG
            if(readed_val!=mem[pos]){
                printf("Error!");
                exit(0);
            }
#endif
            fprintf(result, "%d\n", readed_val);
        }
        else{
            val = rand() % 10;
            write_request(msg, pos, val, 1);
#ifdef DEBUG
            mem[pos] = val;
#endif
        }
    }
    printf("Ending...\n");
#ifdef DEBUG
    printf("Passed test");
#endif
    fclose(result);
    return 0;
}
