/***********************************************************************
 * psim.c - Pipelined Y86 simulator
 * 
 * Copyright (c) 2010, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 ***********************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include "isa.h"
#include "pipeline.h"
#include "stages.h"
#include "sim.h"

#define MAXBUF 1024
#define DEFAULTNAME "Y86 Simulator: "

#define MAXARGS 128
#define MAXBUF 1024
#define TKARGS 3


/***************
 * Begin Globals
 ***************/

/* Simulator name defined and initialized by the compiled HCL file */
/* according to the -n argument supplied to hcl2c */
extern  char simname[];

/* Parameters modifed by the command line */
int gui_mode = FALSE;    /* Run in GUI mode instead of TTY mode? (-g) */
char *object_filename;   /* The input object file name. */
FILE *object_file;       /* Input file handle */
bool_t verbosity = 0;    /* Verbosity level [TTY only] (-v) */ 
int instr_limit = 100000000; /* Instruction limit [TTY only] (-l) */
bool_t do_check = FALSE; /* Test with ISA simulator? [TTY only] (-t) */

//set PSIM_ID=-1 to run in one cpu mode
int PSIM_ID = 0;
int START_PLACE = 0;

/************* 
 * End Globals 
 *************/


/***************************
 * Begin function prototypes 
 ***************************/

static void usage(char *name);           /* Print helpful usage message */
static void run_tty_sim();               /* Run simulator in TTY mode */

/*************************
 * End function prototypes
 *************************/


/*******************************************************************
 * Part 1: This part is the initial entry point that handles general
 * initialization. It parses the command line and does any necessary
 * setup to run in either TTY or GUI mode, and then starts the
 * simulation.
 *******************************************************************/

/* 
 * sim_main - main simulator routine. This function is called from the
 * main() routine in the HCL file.
 */
int sim_main(int argc, char **argv)
{
    int i;
    int c;
    char *myargv[MAXARGS];

    if (argc>2 && PSIM_ID!=-1){
        //the maximum length of memory is 1000
        PSIM_ID = atoi(argv[2]);
        argc-=1;
    }
    if(PSIM_ID == 1)
        START_PLACE = 1000;


    /* Parse the command line arguments */
    while ((c = getopt(argc, argv, "htgl:v:")) != -1) {
        switch(c) {
            case 'h':
                usage(argv[0]);
                break;
            case 'l':
                instr_limit = atoi(optarg);
                break;
            case 'v':
                verbosity = atoi(optarg);
                if (verbosity < 0 || verbosity > 2) {
                    printf("Invalid verbosity %d\n", verbosity);
                    usage(argv[0]);
                }
                break;
            case 't':
                do_check = TRUE;
                break;
            case 'g':
                gui_mode = TRUE;
                break;
            default:
                printf("Invalid option '%c'\n", c);
                usage(argv[0]);
                break;
        }
    }


    /* Do we have too many arguments? */
    if (optind < argc - 1) {
        printf("Too many command line arguments:");
        for (i = optind; i < argc; i++)
            printf(" %s", argv[i]);
        printf("\n");
        usage(argv[0]);
    }


    /* The single unflagged argument should be the object file name */
    object_filename = NULL;
    object_file = NULL;
    if (optind < argc) {
        object_filename = argv[optind];
        object_file = fopen(object_filename, "r");
        if (!object_file) {
            fprintf(stderr, "Couldn't open object file %s\n", object_filename);
            exit(1);
        }
    }

    /* Otherwise, run the simulator in TTY mode (no -g flag) */
    run_tty_sim();

    exit(0);
}

/* 
 * run_tty_sim - Run the simulator in TTY mode
 */
static void run_tty_sim() 
{
    int icount = 0;
    byte_t run_status = STAT_AOK;
    cc_t result_cc = 0;
    int byte_cnt = 0;
    mem_t mem0, reg0;
    state_ptr isa_state = NULL;


    /* In TTY mode, the default object file comes from stdin */
    if (!object_file) {
        object_file = stdin;
    }

    if (verbosity >= 2)
        sim_set_dumpfile(stdout);
    sim_init();

    /* Emit simulator name */
    if (verbosity >= 2)
        printf("%s\n", simname);

    byte_cnt = load_mem(mem, object_file, 1, START_PLACE);
    if (byte_cnt == 0) {
        fprintf(stderr, "No lines of code found\n");
        exit(1);
    } else if (verbosity >= 2) {
        printf("%d bytes of code read\n", byte_cnt);
    }
    fclose(object_file);
    if (do_check) {
        isa_state = new_state(0);
        free_reg(isa_state->r);
        free_mem(isa_state->m);
        isa_state->m = copy_mem(mem);
        isa_state->r = copy_reg(reg);
        isa_state->cc = cc;
    }

    if(verbosity > 0){
        mem0 = copy_mem(mem);
        reg0 = copy_reg(reg);
    }

    icount = sim_run_pipe(instr_limit, 5*instr_limit, &run_status, &result_cc);
    if (verbosity > 0) {
        printf("%d instructions executed\n", icount);
        printf("Status = %s\n", stat_name(run_status));
        printf("Condition Codes: %s\n", cc_name(result_cc));
        printf("Changed Register State:\n");
        diff_reg(reg0, reg, stdout);
        printf("Changed Memory State:\n");
        diff_mem(mem0, mem, stdout);
    }
    if (do_check) {
        exit(0);
        byte_t e = STAT_AOK;
        int step;
        bool_t match = TRUE;

        for (step = 0; step < instr_limit && e == STAT_AOK; step++) {
            e = step_state(isa_state, stdout);
        }

        if (diff_reg(isa_state->r, reg, NULL)) {
            match = FALSE;
            if (verbosity > 0) {
                printf("ISA Register != Pipeline Register File\n");
                diff_reg(isa_state->r, reg, stdout);
            }
        }
        if (diff_mem(isa_state->m, mem, NULL)) {
            match = FALSE;
            if (verbosity > 0) {
                printf("ISA Memory != Pipeline Memory\n");
                diff_mem(isa_state->m, mem, stdout);
            }
        }
        if (isa_state->cc != result_cc) {
            match = FALSE;
            if (verbosity > 0) {
                printf("ISA Cond. Codes (%s) != Pipeline Cond. Codes (%s)\n",
                        cc_name(isa_state->cc), cc_name(result_cc));
            }
        }
        if (match) {
            printf("ISA Check Succeeds\n");
        } else {
            printf("ISA Check Fails\n");
        }
    }

    /* Emit CPI statistics */
    {
        double cpi = instructions > 0 ? (double) cycles/instructions : 1.0;
        printf("%d CPI: %d cycles/%d instructions = %.2f\n", PSIM_ID,
                cycles, instructions, cpi);
    }
}

/*
 * usage - print helpful diagnostic information
 */
static void usage(char *name)
{
    printf("Usage: %s [-htg] [-l m] [-v n] file.yo\n", name);
    printf("file.yo arg required in GUI mode, optional in TTY mode (default stdin)\n");
    printf("   -h     Print this message\n");
    printf("   -g     Run in GUI mode instead of TTY mode (default TTY)\n");  
    printf("   -l m   Set instruction limit to m [TTY mode only] (default %d)\n", instr_limit);
    printf("   -v n   Set verbosity level to 0 <= n <= 2 [TTY mode only] (default %d)\n", verbosity);
    printf("   -t     Test result against ISA simulator [TTY mode only]\n");
    exit(0);
}


/*********************************************************
 * Part 2: This part contains the core simulator routines.
 *********************************************************/


/*****************
 *  Part 2 Globals
 *****************/

/* Performance monitoring */

/* Has simulator gotten past initial bubbles? */
static int starting_up = 1;

/* How many cycles have been simulated? */
int cycles = 0;
/* How many instructions have passed through the WB stage? */
int instructions = 0;

/* Both instruction and data memory */
mem_t mem;
int minAddr = 0;
int memCnt = 0;

/* Register file */
mem_t reg;
/* Condition code register */
cc_t cc;
/* Status code */
stat_t status;


/* Pending updates to state */
word_t cc_in = DEFAULT_CC;
word_t wb_destE = REG_NONE;
word_t wb_valE = 0;
word_t wb_destM = REG_NONE;
word_t wb_testM = REG_NONE;
word_t wb_valM = 0;
word_t mem_addr = 0;
word_t mem_test = 0;
word_t mem_data = 0;
bool_t mem_write = FALSE;

/* EX Operand sources */
mux_source_t amux = MUX_NONE;
mux_source_t bmux = MUX_NONE;

/* Current and next states of all pipeline registers */
pc_ptr pc_curr;
if_id_ptr if_id_curr;
id_ex_ptr id_ex_curr;
ex_mem_ptr ex_mem_curr;
mem_wb_ptr mem_wb_curr;

pc_ptr pc_next;
if_id_ptr if_id_next;
id_ex_ptr id_ex_next;
ex_mem_ptr ex_mem_next;
mem_wb_ptr mem_wb_next;

/* Intermediate values */
word_t f_pc;
byte_t imem_icode;
byte_t imem_ifun;
bool_t imem_error;
bool_t instr_valid;
word_t d_regvala;
word_t d_regvalb;
word_t e_vala;
word_t e_valb;
bool_t e_bcond;
bool_t dmem_error;

/* The pipeline state */
pipe_ptr pc_state, if_id_state, id_ex_state, ex_mem_state, mem_wb_state;

/* Simulator operating mode */
sim_mode_t sim_mode = S_FORWARD;
/* Log file */
FILE *dumpfile = NULL;

/*****************************************************************************
 * reporting code
 *****************************************************************************/

/* Report system state */
static void sim_report() 
{
}

/*****************************************************************************
 * pipeline control
 * These functions can be used to handle hazards
 *****************************************************************************/

/* bubble stage (has effect at next update) */
void sim_bubble_stage(stage_id_t stage) 
{
    switch (stage)
    {
        case IF_STAGE : pc_state->op     = P_BUBBLE; break;
        case ID_STAGE : if_id_state->op  = P_BUBBLE; break;
        case EX_STAGE : id_ex_state->op  = P_BUBBLE; break;
        case MEM_STAGE: ex_mem_state->op = P_BUBBLE; break;
        case WB_STAGE : mem_wb_state->op = P_BUBBLE; break;
    }
}

/* stall stage (has effect at next update) */
void sim_stall_stage(stage_id_t stage) {
    switch (stage)
    {
        case IF_STAGE : pc_state->op     = P_STALL; break;
        case ID_STAGE : if_id_state->op  = P_STALL; break;
        case EX_STAGE : id_ex_state->op  = P_STALL; break;
        case MEM_STAGE: ex_mem_state->op = P_STALL; break;
        case WB_STAGE : mem_wb_state->op = P_STALL; break;
    }
}


static int initialized = 0;
int mem_test_address;

void sim_init()
{
    /* Create memory and register files */
    initialized = 1;
    mem = init_mem_psim_id(MEM_SIZE, PSIM_ID);
    reg = init_reg();
    /* create 5 pipe registers */
    pc_state     = new_pipe(sizeof(pc_ele), (void *) &bubble_pc);
    if_id_state  = new_pipe(sizeof(if_id_ele), (void *) &bubble_if_id);
    id_ex_state  = new_pipe(sizeof(id_ex_ele), (void *) &bubble_id_ex);
    ex_mem_state = new_pipe(sizeof(ex_mem_ele), (void *) &bubble_ex_mem);
    mem_wb_state = new_pipe(sizeof(mem_wb_ele), (void *) &bubble_mem_wb);

    /* connect them to the pipeline stages */
    pc_next   = pc_state->next;
    pc_curr   = pc_state->current;

    if_id_next = if_id_state->next;
    if_id_curr = if_id_state->current;

    id_ex_next = id_ex_state->next;
    id_ex_curr = id_ex_state->current;

    ex_mem_next = ex_mem_state->next;
    ex_mem_curr = ex_mem_state->current;

    mem_wb_next = mem_wb_state->next;
    mem_wb_curr = mem_wb_state->current;

    sim_reset();
    clear_mem(mem);
}

void sim_reset()
{
    if (!initialized)
        sim_init();
    clear_pipes();
    clear_reg(reg);
    minAddr = 0;
    memCnt = 0;
    starting_up = 1;
    cycles = instructions = 0;
    cc = DEFAULT_CC;
    status = STAT_AOK;

    amux = bmux = MUX_NONE;
    cc = cc_in = DEFAULT_CC;
    wb_destE = REG_NONE;
    wb_valE = 0;
    wb_destM = REG_NONE;
    wb_valM = 0;
    mem_addr = 0;
    mem_data = 0;
    mem_write = FALSE;
    sim_report();
}

/* Update state elements */
/* May need to disable updating of memory & condition codes */
static void update_state(bool_t update_mem, bool_t update_cc)
{
    /* Writeback(s):
       If either register is REG_NONE, write will have no effect .
       Order of two writes determines semantics of
       popl %esp.  According to ISA, %esp will get popped value
       */

    if (wb_destE != REG_NONE) {
        sim_log("\tWriteback: Wrote 0x%x to register %s\n",
                wb_valE, reg_name(wb_destE));
        set_reg_val(reg, wb_destE, wb_valE);
        //printf("set_reg_valE %d %d %d\n", PSIM_ID, wb_destE, wb_valE);
        //fflush(stdout);
    }
    if (wb_destM != REG_NONE && wb_testM == REG_NONE) {
        sim_log("\tWriteback: Wrote 0x%x to register %s\n",
                wb_valM, reg_name(wb_destM));
        set_reg_val(reg, wb_destM, wb_valM);
        //printf("set_reg_valM %d %d %d\n", PSIM_ID, wb_destM, wb_valM);
        //fflush(stdout);
    }

    /* Memory write */

    if(wb_testM != REG_NONE){
        set_reg_val(reg, wb_testM, wb_valM);
        printf("PSIM_ID %d test_result:  %d on %d\n", PSIM_ID, wb_valM, mem_test_address);
        //fflush(stdout);
    }

    if (mem_write && !update_mem) {
        sim_log("\tDisabled write of 0x%x to address 0x%x\n", mem_data, mem_addr);
    }
    if (update_mem && mem_write) {
//        printf("WRITE: %d %d\n", mem_addr, mem_data);
        if (!set_word_val(mem, mem_addr, mem_data)) {
            sim_log("\tCouldn't write to address 0x%x\n", mem_addr);
        } else {
            sim_log("\tWrote 0x%x to address 0x%x\n", mem_data, mem_addr);
        }
        /*
        int ans;
        printf("%d %d\n",mem_addr, mem_data);
        int b = get_word_val(mem, mem_addr, &ans);
        printf("%d\n", ans);
        */
    }
    if (update_cc)
        cc = cc_in;
}

/* Text representation of status */
void tty_report(int cyc) {
    sim_log("\nCycle %d. CC=%s, Stat=%s\n", cyc, cc_name(cc), stat_name(status));

    sim_log("F: predPC = 0x%x\n", pc_curr->pc);

    sim_log("D: instr = %s, rA = %s, rB = %s, valC = 0x%x, valP = 0x%x, Stat = %s\n",
            iname(HPACK(if_id_curr->icode, if_id_curr->ifun)),
            reg_name(if_id_curr->ra), reg_name(if_id_curr->rb),
            if_id_curr->valc, if_id_curr->valp,
            stat_name(if_id_curr->status));

    sim_log("E: instr = %s, valC = 0x%x, valA = 0x%x, valB = 0x%x\n   srcA = %s, srcB = %s, dstE = %s, dstM = %s, Stat = %s\n",
            iname(HPACK(id_ex_curr->icode, id_ex_curr->ifun)),
            id_ex_curr->valc, id_ex_curr->vala, id_ex_curr->valb,
            reg_name(id_ex_curr->srca), reg_name(id_ex_curr->srcb),
            reg_name(id_ex_curr->deste), reg_name(id_ex_curr->destm),
            stat_name(id_ex_curr->status));

    sim_log("M: instr = %s, Cnd = %d, valE = 0x%x, valA = 0x%x\n   dstE = %s, dstM = %s, Stat = %s\n",
            iname(HPACK(ex_mem_curr->icode, ex_mem_curr->ifun)),
            ex_mem_curr->takebranch,
            ex_mem_curr->vale, ex_mem_curr->vala,
            reg_name(ex_mem_curr->deste), reg_name(ex_mem_curr->destm),
            stat_name(ex_mem_curr->status));

    sim_log("W: instr = %s, valE = 0x%x, valM = 0x%x, dstE = %s, dstM = %s, Stat = %s\n",
            iname(HPACK(mem_wb_curr->icode, mem_wb_curr->ifun)),
            mem_wb_curr->vale, mem_wb_curr->valm,
            reg_name(mem_wb_curr->deste), reg_name(mem_wb_curr->destm),
            stat_name(mem_wb_curr->status));
}

/* Run pipeline for one cycle */
/* Return status of processor */
/* Max_instr indicates maximum number of instructions that
   want to complete during this simulation run.  */
static byte_t sim_step_pipe(int max_instr, int ccount)
{

    byte_t wb_status = mem_wb_curr->status;
    byte_t mem_status = mem_wb_next->status;
    /* How many instructions are ahead of one in wb / ex? */
    int ahead_mem = (wb_status != STAT_BUB);
    int ahead_ex = ahead_mem + (mem_status != STAT_BUB);
    bool_t update_mem = ahead_mem < max_instr;
    bool_t update_cc = ahead_ex < max_instr;

    /* Update program-visible state */

    update_state(update_mem, update_cc);
    /* Update pipe registers */
    /*
    if(pc_curr->status==P_STALL){
        printf("haha %d %d\n", PSIM_ID, get_reg_val(reg, 0));
    }
    */
    update_pipes();
    tty_report(ccount);
    if (pc_state->op == P_ERROR){
        pc_curr->status = STAT_PIP;
    }
    if (if_id_state->op == P_ERROR)
        if_id_curr->status = STAT_PIP;
    if (id_ex_state->op == P_ERROR)
        id_ex_curr->status = STAT_PIP;
    if (ex_mem_state->op == P_ERROR)
        ex_mem_curr->status = STAT_PIP;
    if (mem_wb_state->op == P_ERROR)
        mem_wb_curr->status = STAT_PIP;

    /* Need to do decode after execute & memory stages,
       and memory stage before execute, in order to propagate
       forwarding values properly */
    do_if_stage();
    do_mem_stage();
    do_ex_stage();
    do_id_wb_stages();

    do_stall_check();
#if 0
    /* This doesn't seem necessary */
    if (id_ex_curr->status != STAT_AOK
            && id_ex_curr->status != STAT_BUB) {
        if_id_state->op = P_BUBBLE;
        id_ex_state->op = P_BUBBLE;
    }
#endif

    /* Performance monitoring */
    if (mem_wb_curr->status != STAT_BUB && mem_wb_curr->icode != I_POP2) {
        starting_up = 0;
        instructions++;
        cycles++;
    } else {
        if (!starting_up)
            cycles++;
    }

    sim_report();
    return status;
}

/*
   Run pipeline until one of following occurs:
   - An error status is encountered in WB.
   - max_instr instructions have completed through WB
   - max_cycle cycles have been simulated

   Return number of instructions executed.
   if statusp nonnull, then will be set to status of final instruction
   if ccp nonnull, then will be set to condition codes of final instruction
   */
int sim_run_pipe(int max_instr, int max_cycle, byte_t *statusp, cc_t *ccp)
{

    int icount = 0;
    int ccount = 0;
    byte_t run_status = STAT_AOK;
    while (icount < max_instr && ccount < max_cycle) {
        run_status = sim_step_pipe(max_instr-icount, ccount);
        if (run_status != STAT_BUB)
            icount++;
        if (run_status != STAT_AOK && run_status != STAT_BUB)
            break;
        ccount++;
    }
    if (statusp)
        *statusp = run_status;
    if (ccp)
        *ccp = cc;
    return icount;
}

/* If dumpfile set nonNULL, lots of status info printed out */
void sim_set_dumpfile(FILE *df)
{
    dumpfile = df;
}

/*
 * sim_log dumps a formatted string to the dumpfile, if it exists
 * accepts variable argument list
 */
void sim_log( const char *format, ... ) {
    if (dumpfile) {
        va_list arg;
        va_start( arg, format );
        vfprintf( dumpfile, format, arg );
        va_end( arg );
    }
}


/**************************************************************
 * Part 4: Code for implementing pipelined processor simulators
 *************************************************************/

/******************************************************************************
 *	defines
 ******************************************************************************/

#define MAX_STAGE 10

/******************************************************************************
 *	static variables
 ******************************************************************************/

static pipe_ptr pipes[MAX_STAGE];
static int pipe_count = 0;

/******************************************************************************
 *	function definitions
 ******************************************************************************/

/* Create new pipe with count bytes of state */
/* bubble_val indicates state corresponding to pipeline bubble */
pipe_ptr new_pipe(int count, void *bubble_val)
{
    pipe_ptr result = (pipe_ptr) malloc(sizeof(pipe_ele));
    result->current = malloc(count);
    result->next = malloc(count);
    memcpy(result->current, bubble_val, count);
    memcpy(result->next, bubble_val, count);
    result->count = count;
    result->op = P_LOAD;
    result->bubble_val = bubble_val;
    pipes[pipe_count++] = result; 
    return result;
}

/* Update all pipes */
void update_pipes()
{
    int s;
    for (s = 0; s < pipe_count; s++) {
        pipe_ptr p = pipes[s];
        switch (p->op)
        {
            case P_BUBBLE:
                /* insert a bubble into the next stage */
                memcpy(p->current, p->bubble_val, p->count);
                break;

            case P_LOAD:
                /* copy calculated state from previous stage */
                memcpy(p->current, p->next, p->count);
                break;
            case P_ERROR:
                /* Like a bubble, but insert error condition */
                memcpy(p->current, p->bubble_val, p->count);
                break;
            case P_STALL:
            default:
                /* do nothing: next stage gets same instr again */
                ;
        }
        if (p->op != P_ERROR)
            p->op = P_LOAD;
    }
}

/* Set all pipes to bubble values */
void clear_pipes()
{
    int s;
    for (s = 0; s < pipe_count; s++) {
        pipe_ptr p = pipes[s];
        memcpy(p->current, p->bubble_val, p->count);
        memcpy(p->next, p->bubble_val, p->count);
        p->op = P_LOAD;
    }
}

/******************** Utility Code *************************/

/* Representations of digits */
static char digits[16] =
{'0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

/* Print hex/oct/binary format with leading zeros */
/* bpd denotes bits per digit  Should be in range 1-4,
   pbw denotes bits per word.*/
void wprint(unsigned x, int bpd, int bpw, FILE *fp)
{
    int digit;
    unsigned mask = (1 << bpd) - 1;
    for (digit = (bpw-1)/bpd; digit >= 0; digit--) {
        unsigned val = (x >> (digit * bpd)) & mask;
        putc(digits[val], fp);
    }
}

/* Create string in hex/oct/binary format with leading zeros */
/* bpd denotes bits per digit  Should be in range 1-4,
   pbw denotes bits per word.*/
void wstring(unsigned x, int bpd, int bpw, char *str)
{
    int digit;
    unsigned mask = (1 << bpd) - 1;
    for (digit = (bpw-1)/bpd; digit >= 0; digit--) {
        unsigned val = (x >> (digit * bpd)) & mask;
        *str++ = digits[val];
    }
    *str = '\0';
}


/********************************
 * Part 5: Stage implementations
 *********************************/

/*************** Bubbled version of stages *************/

pc_ele bubble_pc = {0,STAT_AOK};
if_id_ele bubble_if_id = { I_NOP, 0, REG_NONE,REG_NONE,
    0, 0, STAT_BUB, 0};
id_ex_ele bubble_id_ex = { I_NOP, 0, 0, 0, 0,
    REG_NONE, REG_NONE, REG_NONE, REG_NONE,
    STAT_BUB, 0};

ex_mem_ele bubble_ex_mem = { I_NOP, 0, FALSE, 0, 0,
    REG_NONE, REG_NONE, STAT_BUB, 0};

mem_wb_ele bubble_mem_wb = { I_NOP, 0, 0, 0, REG_NONE, REG_NONE,
    STAT_BUB, 0};

/*************** Stage Implementations *****************/

int gen_f_pc();
int gen_need_regids();
int gen_need_valC();
int gen_instr_valid();
int gen_f_predPC();
int gen_f_icode();
int gen_f_ifun();
int gen_f_stat();
int gen_instr_valid();

void do_if_stage()
{
    byte_t instr = HPACK(I_NOP, F_NONE);
    byte_t regids = HPACK(REG_NONE, REG_NONE);
    word_t valc = 0;
    word_t valp = f_pc = gen_f_pc();

    /* Ready to fetch instruction.  Speculatively fetch register byte
       and immediate word
       */
    imem_error = !get_byte_val(mem, valp+START_PLACE, &instr);
    imem_icode = HI4(instr);
    imem_ifun = LO4(instr);
    if (!imem_error) {
        byte_t junk;
        /* Make sure can read maximum length instruction */
        imem_error = !get_byte_val(mem, valp+5 + START_PLACE, &junk);
    }
    if_id_next->icode = gen_f_icode();
    if_id_next->ifun  = gen_f_ifun();
    if (!imem_error) {
        sim_log("\tFetch: f_pc = 0x%x, imem_instr = %s, f_instr = %s\n",
                f_pc, iname(instr),
                iname(HPACK(if_id_next->icode, if_id_next->ifun)));
    }

    instr_valid = gen_instr_valid();
    if (!instr_valid) 
        sim_log("\tFetch: Instruction code 0x%x invalid\n", instr);
    if_id_next->status = gen_f_stat();

    valp++;
    if (gen_need_regids()) {
        get_byte_val(mem, valp + START_PLACE, &regids);
        valp ++;
    }
    if_id_next->ra = HI4(regids);
    if_id_next->rb = LO4(regids);
    if (gen_need_valC()) {
        get_word_val(mem, valp + START_PLACE, &valc);
        valp+= 4;
    }
    if_id_next->valp = valp;
    if_id_next->valc = valc;
    /*
    if(gen_need_regids() && if_id_next->valc)
        printf("##### vap: %d %d %d\n", if_id_next->ra, if_id_next->rb, if_id_next->valc);
        */

    pc_next->pc = gen_f_predPC();

    pc_next->status = (if_id_next->status == STAT_AOK) ? STAT_AOK : STAT_BUB;

    if_id_next->stage_pc = f_pc;
}

int gen_d_srcA();
int gen_d_srcB();
int gen_d_dstE();
int gen_d_dstM();
int gen_d_valA();
int gen_d_valB();
int gen_w_dstE();
int gen_w_valE();
int gen_w_dstM();
int gen_w_valM();
int gen_Stat();

/* Implements both ID and WB */
void do_id_wb_stages()
{
    /* Set up write backs.  Don't occur until end of cycle */
    wb_destE = gen_w_dstE();
    wb_valE = gen_w_valE();
    wb_destM = gen_w_dstM();
    wb_valM = gen_w_valM();
    if(mem_wb_curr->icode == I_TEST)
        wb_testM = wb_destM;
    else
        wb_testM = REG_NONE;

    /* Update processor status */
    status = gen_Stat();

    id_ex_next->srca = gen_d_srcA();
    id_ex_next->srcb = gen_d_srcB();
    id_ex_next->deste = gen_d_dstE();
    id_ex_next->destm = gen_d_dstM();


    /* Read the registers */
    d_regvala = get_reg_val(reg, id_ex_next->srca);
    d_regvalb = get_reg_val(reg, id_ex_next->srcb);
    //printf("%d %d %d %d\n", id_ex_next->srca, id_ex_next->srcb, d_regvala, d_regvalb);

    /* Do forwarding and valA selection */
    id_ex_next->vala = gen_d_valA();
    id_ex_next->valb = gen_d_valB();

    id_ex_next->icode = if_id_curr->icode;
    id_ex_next->ifun = if_id_curr->ifun;
    id_ex_next->valc = if_id_curr->valc;
    id_ex_next->stage_pc = if_id_curr->stage_pc;
    id_ex_next->status = if_id_curr->status;
}

int gen_alufun();
int gen_set_cc();
int gen_Bch();
int gen_aluA();
int gen_aluB();
int gen_e_valA();
int gen_e_dstE();

void do_ex_stage()
{
    alu_t alufun = gen_alufun();
    bool_t setcc = gen_set_cc();
    word_t alua, alub;

    alua = gen_aluA();
    alub = gen_aluB();
    e_bcond =	cond_holds(cc, id_ex_curr->ifun);

    ex_mem_next->takebranch = e_bcond;

    if (id_ex_curr->icode == I_JMP)
        sim_log("\tExecute: instr = %s, cc = %s, branch %staken\n",
                iname(HPACK(id_ex_curr->icode, id_ex_curr->ifun)),
                cc_name(cc),
                ex_mem_next->takebranch ? "" : "not ");

    /* Perform the ALU operation */
    word_t aluout = compute_alu(alufun, alua, alub);
    ex_mem_next->vale = aluout;
    sim_log("\tExecute: ALU: %c 0x%x 0x%x --> 0x%x\n",
            op_name(alufun), alua, alub, aluout);

    if (setcc) {
        cc_in = compute_cc(alufun, alua, alub);
        sim_log("\tExecute: New cc = %s\n", cc_name(cc_in));
    }

    ex_mem_next->icode = id_ex_curr->icode;
    ex_mem_next->ifun = id_ex_curr->ifun;
    ex_mem_next->vala = gen_e_valA();
    ex_mem_next->deste = gen_e_dstE();
    ex_mem_next->destm = id_ex_curr->destm;
    ex_mem_next->srca = id_ex_curr->srca;
    ex_mem_next->status = id_ex_curr->status;
    ex_mem_next->stage_pc = id_ex_curr->stage_pc;
}

/* Functions defined using HCL */
int gen_mem_addr();
int gen_mem_read();
int gen_mem_write();
int gen_m_stat();

void do_mem_stage()
{
    bool_t read = gen_mem_read();

    word_t valm = 0;

    mem_addr = gen_mem_addr();
    mem_data = ex_mem_curr->vala;
    mem_write = gen_mem_write();
    mem_test = gen_mem_test();
    dmem_error = FALSE;

    if (read && !mem_test) {
        dmem_error = dmem_error || !get_word_val(mem, mem_addr, &valm);
        if (!dmem_error)
            sim_log("\tMemory: Read 0x%x from 0x%x\n",
                    valm, mem_addr);
    }
    if (mem_write && !mem_test) {
        word_t sink;
        /* Do a read of address just to check validity */
        dmem_error = dmem_error || !get_word_val(mem, mem_addr, &sink);
        if (dmem_error)
            sim_log("\tMemory: Invalid address 0x%x\n",
                    mem_addr);
    }
    if (mem_test && read){
        mem_test_address = mem_addr;

        int ans = test_memory(mem, mem_test_address);
        valm = ans;
    }
    mem_wb_next->icode = ex_mem_curr->icode;
    mem_wb_next->ifun = ex_mem_curr->ifun;
    mem_wb_next->vale = ex_mem_curr->vale;
    mem_wb_next->valm = valm;
    mem_wb_next->deste = ex_mem_curr->deste;
    mem_wb_next->destm = ex_mem_curr->destm;
    mem_wb_next->status = gen_m_stat();
    mem_wb_next->stage_pc = ex_mem_curr->stage_pc;
}

/* Set stalling conditions for different stages */

int gen_F_stall(), gen_F_bubble();
int gen_D_stall(), gen_D_bubble();
int gen_E_stall(), gen_E_bubble();
int gen_M_stall(), gen_M_bubble();
int gen_W_stall(), gen_W_bubble();

p_stat_t pipe_cntl(char *name, int stall, int bubble)
{
    if (stall) {
        if (bubble) {
            sim_log("%s: Conflicting control signals for pipe register\n",
                    name);
            return P_ERROR;
        } else 
            return P_STALL;
    } else {
        return bubble ? P_BUBBLE : P_LOAD;
    }
}

void do_stall_check()
{
    /*
    if(gen_D_stall()){
        printf("%d D_stall\n", PSIM_ID);
    }
    if(gen_F_stall()){
        printf("%d stall\n", PSIM_ID);
    }
    */
    pc_state->op = pipe_cntl("PC", gen_F_stall(), gen_F_bubble());
    if_id_state->op = pipe_cntl("ID", gen_D_stall(), gen_D_bubble());
    id_ex_state->op = pipe_cntl("EX", gen_E_stall(), gen_E_bubble());
    ex_mem_state->op = pipe_cntl("MEM", gen_M_stall(), gen_M_bubble());
    mem_wb_state->op = pipe_cntl("WB", gen_W_stall(), gen_W_bubble());
}
