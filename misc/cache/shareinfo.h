#include<sys/shm.h>
#include<sys/types.h>
#include<errno.h>
#include<stdio.h>
#include<sys/ipc.h> 
#include<stdlib.h>
#define MAX_MEM_POS 30000
#define MAX_CACHE_LEN 32
#define BYTE_PER_CACHE 64
#define MAX_BUS_LEN 32
#define MEM_NAME (ftok("Makefile",44))
#define BUS_NAME (ftok("Makefile", 43))
#define MSG_NAME(x) (ftok("Makefile", (x)+10))
