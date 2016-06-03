#include<stdio.h>
#include<sys/shm.h>
#include<time.h>
#include<stdlib.h>
#include<sys/types.h>
#include<errno.h>
#include<sys/ipc.h> 
#include "shareinfo.h"
#include<stdlib.h>
#define PERM S_IRUSR|S_IWUSR 

typedef unsigned char byte_t;

typedef struct{
    int max_len;
    int* key;
    int val[MAX_CACHE_LEN][BYTE_PER_CACHE];
    int* flag;
    int* hit_time;
    volatile byte_t* mem; 
    volatile int* bus;
    int id, num;
}cache_rec, *cache_t;

/*
 * bus
 * 0
 * 1 2 please lock
 * 3 4 ask
 * 5 6 answer
 * 9 10 ask_type_bit
 * 11 12 ask_pos_bit
 * 13 14 ask_val_bit
 * 17 18 extra_bit
 */

int ask_bit(int id){
    return id + 3;
}

int answer_bit(int id){
    return id + 5;
}

int ask_type_bit(int id){
    return id + 9;
}

int ask_pos_bit(int id){
    return id + 11;
}

int ask_val_bit(int id){
    return id + 13;
}

int lock_bit(int id){
    return id + 1;
}

int extra_bit(int id){
    return id + 17;
}

int stop_ask_bit(int id){
    return id + 7;
}

/*
 * 0 1 2 3
 * M E S I
 * */

#define READ 0
#define WRITE 1
#define TEST 2
#define TYPE_M 0
#define TYPE_E 1
#define TYPE_S 2
#define TYPE_I (-1)

int find_cache_line(cache_t c, int pos){
    int i;
    pos = pos/BYTE_PER_CACHE * BYTE_PER_CACHE;
    for(i=0;i<c->num;++i)
        if(c->key[i]==pos && c->flag[i]!=TYPE_I){
            return i;
        }
    return TYPE_I;
}

void cache_to_mem(cache_t c, int idx){
    int i;
    for(i=0;i<BYTE_PER_CACHE;++i){
        c->mem[ c->key[idx] + i ] = c->val[idx][i];
    }
}

void mem_to_cache(cache_t c, int idx){
    int i;
    for(i=0;i<BYTE_PER_CACHE;++i){
        c->val[idx][i] = c->mem[ c->key[idx] + i ];
    }
}

void write_cache_val(cache_t c, int idx, int pos, int val){
    c->val[idx][pos - c->key[idx]] = val;
}

int read_cache_val(cache_t c, int idx, int pos){
    return c->val[idx][ pos - c->key[idx] ];
}

int new_cache_line(cache_t c, int pos){
    //get a cache line to write
    //if there is free or Invalid cache, write to it
    //otherwise replace
    int i;
    pos = pos/BYTE_PER_CACHE * BYTE_PER_CACHE;
    for(i=0;i<c->num;++i)
        if(c->flag[i] == TYPE_I){
            c->hit_time[i] = 0;
            c->key[i] = pos;
            return i;
        }
    if(c->num<c->max_len){
        i = c->num;
        c->flag[i] = TYPE_I;
        c->hit_time[i] = 0;
        c->key[i] = pos;
        c->num++;
        return i;
    }
    int min_hit_time = c->hit_time[0], ans=0;
    for(i=0;i<c->num;++i)
        if(c->hit_time[i]<min_hit_time){
            ans = i;
            min_hit_time = c->hit_time[i];
        }
    if(c->flag[ans] == TYPE_M){
        cache_to_mem(c, ans);
    }
    c->hit_time[ans] = 0;
    c->flag[ans] = TYPE_I;
    c->key[ans] = pos;
    return ans;
}

int load_data(cache_t c, int pos){
    //doesn't care about the type and any thing
    //load data from mem
    int idx = find_cache_line(c, pos);
    if(idx == TYPE_I){
        idx = new_cache_line(c, pos);
        mem_to_cache(c, idx);
    }
    else{
        exit(0);
    }
    return idx;
}

void Remote(cache_t c, int pos, int TYPE){
    int idx = find_cache_line(c, pos);
    if(idx == TYPE_I){
        if(TYPE==READ){
        }
        else if(TYPE==WRITE){
        }
    }
    else if(c->flag[idx] == TYPE_E){
        if(TYPE == READ)
            c->flag[idx]= TYPE_S;
        else{
            c->flag[idx] = TYPE_I;
        }
    }
    else if(c->flag[idx] == TYPE_S){
        if(TYPE == READ){
        }
        else{
            c->flag[idx] = TYPE_I;
        }
    }
    else if(c->flag[idx] == TYPE_M){
        cache_to_mem(c, idx);
        if(TYPE == READ){
            c->flag[idx] = TYPE_S;
        }
        else{
            c->flag[idx] = TYPE_I;
        }
    }
    c->bus[extra_bit(c->id^1)] = (idx==TYPE_I?TYPE_I:c->flag[idx]);
}

int Local(cache_t c, int pos, int val, int TYPE){
    int idx = find_cache_line(c, pos);
    int READ_VAL;
    if(idx == TYPE_I){
        idx = load_data(c, pos);
        if(TYPE == READ){
            int other_type = c->bus[extra_bit(c->id)];
            if(other_type == TYPE_I){
                c->flag[idx] = TYPE_E;
            }
            else{
                c->flag[idx] = TYPE_S;
            }
            READ_VAL = read_cache_val(c, idx, pos);
        }
        else if(TYPE==WRITE){
            write_cache_val(c, idx, pos, val);
            c->flag[idx] = TYPE_M;
        }
    }
    else if(c->flag[idx] == TYPE_E){
        if(TYPE==READ)
            READ_VAL = read_cache_val(c, idx, pos);
        else if(TYPE==WRITE){
            write_cache_val(c, idx, pos, val);
            c->flag[idx] = TYPE_M;
        }
    }
    else if(c->flag[idx] == TYPE_S){
        if(TYPE==READ)
            READ_VAL = read_cache_val(c, idx, pos);
        else if(TYPE==WRITE){
            write_cache_val(c, idx, pos, val);
            c->flag[idx] = TYPE_M;
        }
    }
    else if(c->flag[idx] == TYPE_M){
        if(TYPE==READ)
            READ_VAL = read_cache_val(c, idx, pos);
        else if(TYPE==WRITE){
            write_cache_val(c, idx, pos, val);
        }
    }
#ifdef DEBUG
    FILE* tmpf = fopen("cache_record.txt", "a");
    if(TYPE==READ)
        val = READ_VAL;
    fprintf(tmpf, "%d %d %d %d\n", c->id, TYPE, pos, val);
    fclose(tmpf);

    /*
    tmpf = fopen("mem_tmp.txt", "a");
    int i=0;
    for(i=0;i<10;i++){
        fprintf(tmpf, "%d ", c->mem[i]);
    }
    fprintf(tmpf, "\n");
    fclose(tmpf);
    */
#endif
    c->hit_time[idx] += 1;
    if(TYPE==READ)
        return READ_VAL;
    return 0;
}

void answer(cache_t c){
    if(c->bus[lock_bit(c->id^1)]){
        int askNum = c->bus[ ask_bit(c->id^1) ];
        while(askNum--){
            while(c->bus[ask_bit(c->id^1)]==0){
            }
            c->bus[ask_bit(c->id^1)] = 0;
            int TYPE = c->bus[ask_type_bit(c->id^1)];
            int pos = c->bus[ask_pos_bit(c->id^1)];
            Remote(c, pos, TYPE);
            c->bus[answer_bit(c->id)] = 1;
            while(c->bus[stop_ask_bit(c->id^1)]){
            }
            c->bus[answer_bit(c->id)] = 0;
        }
    }
}

int ask(cache_t c, int pos, int val, int TYPE, int length){
    int ans=0;
    int askNum = length;
    if(TYPE == TEST)
        askNum *= 2;

    while(askNum){
        int newtype = (TYPE==TEST)?(askNum>length?READ:WRITE):TYPE;
        int delta = (askNum-1)%length;
        int newpos = pos + delta;
        if(TYPE==TEST && newtype == WRITE){
            val = ans+1;
        }
        int newval = (val>>(delta*8))%256;

        c->bus[ask_pos_bit(c->id)] = newpos;
        c->bus[ask_val_bit(c->id)] = newval;
        c->bus[ask_type_bit(c->id)] = newtype;
        c->bus[answer_bit(c->id^1)] = 0;
        c->bus[stop_ask_bit(c->id)] = 1;
        c->bus[ask_bit(c->id)] = askNum;

        while(c->bus[answer_bit(c->id^1)]==0 && c->bus[lock_bit(c->id^1)]){
            if(c->bus[ ask_bit(c->id^1)] && (c->id > (c->id^1))){
                answer(c);
            }
        }
        int tmp = Local(c, newpos, newval, newtype);
        if(newtype == READ)
            ans = ans*256 + tmp;
        c->bus[stop_ask_bit(c->id)] = 0;
        c->bus[ask_bit(c->id)]=0;
        while(c->bus[answer_bit(c->id^1)]){
        }
        askNum--;
    }
    return ans;
}

int ONE_STEP(cache_t c, int pos, int val, int TYPE, int length){
    if(pos<0 || pos >= MAX_MEM_POS/BYTE_PER_CACHE * BYTE_PER_CACHE){
        printf("pos for cache is greater than maximum memory %d or less than 0: %d\n", MAX_MEM_POS, pos);
        exit(0);
    }
    answer(c);
    int ans = ask(c, pos, val, TYPE, length);
    return ans;
}


cache_t init_cache(int len, int id, volatile byte_t* mem, volatile int* bus){
    cache_t result = (cache_t) malloc(sizeof(cache_rec));
    result->max_len = len;
    result->key =(int*)calloc(len, sizeof(int));
    result->flag =(int*)calloc(len, sizeof(int));
    result->hit_time =(int*)calloc(len, sizeof(int));

    result->bus = bus;
    result->mem = mem;
    result->num = 0;
    result->id = id;
    bus[lock_bit(id)] = 1;
    return result;
}

int read_request(volatile int* msg, int pos, int length){
    msg[1] = pos;
    msg[0] = 1;
    msg[4] = length;
    while(msg[0]){
    }
    return msg[3];
}

void write_request(volatile int* msg, int pos, int val, int length){
    msg[1] = pos;
    msg[2] = val;
    msg[0] = 2;
    msg[4] = length;
    while(msg[0]){
    }
}

int test_request(volatile int* msg, int pos, int length){
    msg[1] = pos;
    msg[0] = 3;
    msg[4] = length;
    while(msg[0]){
    }
    return msg[3];
}
