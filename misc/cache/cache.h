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
    int* val;
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

#define ASK_WIRTE_SHARED_STILL_SHARED 0

int find_cache_line(cache_t c, int pos){
    int i=0;
    for(i=0;i<c->num;++i)
        if(c->key[i]==pos && c->flag[i]!=TYPE_I){
            return i;
        }
    return TYPE_I;
}

void memory_write(cache_t c, int pos, int val){
    c->mem[pos] = val;
}

int new_cache_line(cache_t c, int pos){
    //get a cache line to write
    //if there is free or Invalid cache, write to it
    //otherwise replace
    int i;
    if(c->num<c->max_len){
        i = c->num;
        c->flag[i] = TYPE_I;
        c->hit_time[i] = 1;
        c->num++;
        return i;
    }
    for(i=0;i<c->num;++i){
        if(c->flag[i] != TYPE_I && c->key[i] == pos){
            printf("No need to get new cache line: already in cache!");
            exit(0);
        }
    }
    for(i=0;i<c->num;++i)
        if(c->flag[i] == TYPE_I){
            c->hit_time[i] = 1;
            return i;
        }
    if(c->max_len ==0){
        printf("You are SB\n");
        exit(0);
    }
    int min_hit_time = c->hit_time[0], ans=0;
    for(i=0;i<c->num;++i)
        if(c->hit_time[i]<min_hit_time){
            ans = i;
            min_hit_time = c->hit_time[i];
        }
    if(c->flag[ans] == TYPE_M){
        // I don't know whether this is safe.
        memory_write(c, c->key[ans], c->val[ans]);
    }
    c->flag[ans] = TYPE_I;
    return ans;
}

int load_data(cache_t c, int pos){
    //doesn't care about the type and any thing
    //load data from mem
    int val = c->mem[pos];
    int idx = find_cache_line(c, pos);
    if(idx == TYPE_I){
        idx = new_cache_line(c, pos);
        c->key[idx] = pos;
        c->val[idx] = val;
    }
    else{
        c->val[idx] = val;
    }
    return idx;
}

void Remote(cache_t c, int pos, int val, int TYPE){
    int idx = find_cache_line(c, pos);
    if(idx == TYPE_I){
        if(TYPE==READ){
        }
        else if(TYPE==WRITE){
        }
    }
    else if(c->flag[idx] == TYPE_E){
        if(TYPE == READ)
            c->flag[idx]=TYPE_S;
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
        memory_write(c, c->key[idx], c->val[idx]);
        if(TYPE == READ){
            c->flag[idx] = TYPE_S;
        }
        else{
            c->flag[idx] = TYPE_I;
        }
    }
    c->bus[extra_bit(c->id^1)] = (idx==TYPE_S?TYPE_S:c->flag[idx]);
}

int Local(cache_t c, int pos, int val, int TYPE){
#ifdef DEBUG
    FILE* tmpf = fopen("cache_record.txt", "a");
    fprintf(tmpf, "%d %d %d %d\n", c->id, TYPE, pos, val);
    fclose(tmpf);

    tmpf = fopen("mem_tmp.txt", "a");
    int i=0;
    for(i=0;i<10;i++){
        fprintf(tmpf, "%d ", c->mem[i]);
    }
    fprintf(tmpf, "\n");
    fclose(tmpf);
#endif
    int idx = find_cache_line(c, pos);
    int READ_VAL;
    if(idx == TYPE_I){
        if(TYPE == READ){
            int other_type = c->bus[extra_bit(c->id)];
            idx = load_data(c, pos);
            //printf("%d %d %d %d\n", c->id, idx, pos, c->val[idx]);
            READ_VAL = c->val[idx];
            if(other_type == TYPE_I){
                c->flag[idx] = TYPE_E;
            }
            else{
                c->flag[idx] = TYPE_S;
            }
        }
        else if(TYPE==WRITE){
            idx = load_data(c, pos);
            c->val[idx] = val;
            c->flag[idx] = TYPE_M;
        }
    }
    else if(c->flag[idx] == TYPE_E){
        if(TYPE==READ)
            READ_VAL = c->val[idx];
        else if(TYPE==WRITE){
            c->val[idx] = val;
            c->flag[idx] = TYPE_M;
        }
    }
    else if(c->flag[idx] == TYPE_S){
        if(TYPE==READ)
            READ_VAL = c->val[idx];
        else if(TYPE==WRITE){
            c->val[idx] = val;
            c->flag[idx] = TYPE_M;
        }
    }
    else if(c->flag[idx] == TYPE_M){
        if(TYPE==READ)
            READ_VAL = c->val[idx];
        else if(TYPE==WRITE){
            c->val[idx] = val;
        }
    }
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
            int val = c->bus[ask_val_bit(c->id^1)];
            Remote(c, pos, val, TYPE);
            c->bus[answer_bit(c->id)] = 1;
        }
    }
}

int ask(cache_t c, int pos, int val, int TYPE){
    int idx = find_cache_line(c, pos);
    int onlylocal = 0, ans=0;
    if(idx!=TYPE_I && 
            (c->flag[idx] == TYPE_M || 
             c->flag[idx] == TYPE_E || 
             (c->flag[idx] == TYPE_S && TYPE == READ )))
        onlylocal = 1;
    if(c->bus[lock_bit(c->id^1)] == 0 || onlylocal){
        if(TYPE!=TEST){
            ans = Local(c, pos, val, TYPE);
            return ans;
        }
        else if(c->bus[lock_bit(c->id^1)] == 0){
            /*
             * here is a bug
             * ....
             * sad
             */
            int ans = Local(c, pos, val, READ);
            Local(c, pos, ans+1, WRITE);
            return ans;
        }
    }
    int askNum = 1+(TYPE==TEST)*7;

    while(askNum){
        int newtype = TYPE, newpos = pos, newval = val;
        if(TYPE==TEST){
            int now = askNum;
            newtype = (now>4)?READ: WRITE;
            if(now>4){
                newpos = pos + now - 5;
            }
            else{
                newpos = pos + now-1;
            }
            if(now<=4){
                newval = ((ans+1)>>((now-1)*8))%256;
            }
        }
        c->bus[ask_pos_bit(c->id)] = pos;
        c->bus[ask_val_bit(c->id)] = val;
        c->bus[ask_type_bit(c->id)] = newtype;
        c->bus[answer_bit(c->id^1)] = 0;
        c->bus[ask_bit(c->id)] = askNum;

        while(c->bus[answer_bit(c->id^1)]==0 && c->bus[lock_bit(c->id^1)]){
            if(c->bus[ ask_bit(c->id^1)] && (c->id > (c->id^1))){
                answer(c);
            }
        }
        int tmp = Local(c, newpos, newval, newtype);
        if(newtype == READ){
            ans = ans*256 + tmp;
        }
        askNum -= 1;
    }
    return ans;
}

int ONE_STEP(cache_t c, int pos, int val, int TYPE){
    if(pos<0 || pos > MAX_MEM_POS){
        printf("pos for cache is greater than maximum memory %d or less than 0: %d\n", MAX_MEM_POS, pos);
        exit(0);
    }
    answer(c);
    int ans = ask(c, pos, val, TYPE);
    return ans;
}


cache_t init_cache(int len, int id, volatile byte_t* mem, volatile int* bus){
    cache_t result = (cache_t) malloc(sizeof(cache_rec));
    result->max_len = len;
    result->key =(int*)calloc(len, sizeof(int));
    result->val =(int*)calloc(len, sizeof(int));
    result->flag =(int*)calloc(len, sizeof(int));
    result->hit_time =(int*)calloc(len, sizeof(int));
    result->val[0]=0;

    result->bus = bus;
    result->mem = mem;
    result->num = 0;
    result->id = id;
    bus[lock_bit(id)] = 1;
    return result;
}

int read_request(volatile int* msg, int pos){
    msg[1] = pos;
    msg[0] = 1;
    while(msg[0]){
    }
    return msg[3];
}

void write_request(volatile int* msg, int pos, int val){
    msg[1] = pos;
    msg[2] = val;
    msg[0] = 2;
    while(msg[0]){
    }
}

int test_request(volatile int* msg, int pos){
    msg[1] = pos;
    msg[0] = 3;
    while(msg[0]){
    }
    return msg[3];
}
