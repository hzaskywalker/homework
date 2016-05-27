#include<sys/shm.h>
#include<sys/types.h>
#include<errno.h>
#include<stdio.h>
#include<sys/ipc.h> 
#include<stdlib.h>
#define MAX_MEM_POS 30000
#define MAX_CACHE_LEN 32
#define MAX_BUS_LEN 32
#define MEM_NAME (ftok("/Users/hza/homework/Makefile",44))
#define BUS_NAME (ftok("/Users/hza/homework/Makefile", 43))
#define MSG_NAME(x) (ftok("/Users/hza/homework/Makefile", x))
