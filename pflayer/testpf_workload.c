/* testpf_workload.c: Runs a random access workload */
#include <stdio.h>
#include <stdlib.h> /* For rand, srand, exit, atof */
#include <string.h> /* For strcmp */
#include <time.h>   /* For time() */
#include "pf.h"

/* CONFIGURATION */
#define TESTFILE "workload_file"
#define BUFFER_SIZE 20      /* 20-page buffer */
#define FILE_SIZE 100       /* 100-page file (larger than buffer) */
#define TOTAL_ACCESSES 10000  /* Number of random requests */

/*
 * Prints stats in a CSV (Comma-Separated Value) format
 * This makes it easy to copy into a spreadsheet
 */
void print_stats(const char* strategy_name, double write_mix)
{
    long logical, physical_r, physical_w;
    PF_GetStats(&logical, &physical_r, &physical_w);
    
    long total_physical = physical_r + physical_w;
    double hit_rate = (logical > 0) ? (100.0 * (logical - physical_r) / logical) : 0;
    
    /* Format: Strategy, WriteMix, Logical, PhysicalReads, PhysicalWrites, TotalPhysical, HitRate */
    printf("%s,%.2f,%ld,%ld,%ld,%ld,%.2f\n",
           strategy_name, write_mix, logical, physical_r, physical_w, total_physical, hit_rate);
}


int main(int argc, char *argv[])
{
    int fd, i, error;
    int pagenum;
    char *buf;

    /* --- 1. Argument Parsing --- */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <strategy: lru|mru> <write_mix_float: 0.0 to 1.0>\n", argv[0]);
        return 1;
    }
    
    char* strategy_str = argv[1];
    double write_mix = atof(argv[2]); /* Convert string to float */
    int strategy;

    if (strcmp(strategy_str, "lru") == 0) {
        strategy = PF_STRAT_LRU;
    } else if (strcmp(strategy_str, "mru") == 0) {
        strategy = PF_STRAT_MRU;
    } else {
        fprintf(stderr, "Error: Strategy must be 'lru' or 'mru'.\n");
        return 1;
    }
    
    if (write_mix < 0.0 || write_mix > 1.0) {
        fprintf(stderr, "Error: Write mix must be between 0.0 and 1.0.\n");
        return 1;
    }

    /* --- 2. Setup --- */
    srand(time(NULL)); /* Seed the random number generator */
    PF_SetBufferSize(BUFFER_SIZE);
    PF_Init();
    PF_SetStrategy(strategy);
    PF_DestroyFile(TESTFILE); /* Clean up from any previous run */

    /* Create and populate the file */
    if (PF_CreateFile(TESTFILE) != PFE_OK) { PF_PrintError("PF_CreateFile"); exit(1); }
    if ((fd = PF_OpenFile(TESTFILE)) < 0) { PF_PrintError("PF_OpenFile"); exit(1); }
    
    for (i = 0; i < FILE_SIZE; i++) {
        if (PF_AllocPage(fd, &pagenum, &buf) != PFE_OK) { PF_PrintError("PF_AllocPage"); exit(1); }
        *buf = (char)('A' + (i % 26)); /* Write some data */
        if (PF_UnfixPage(fd, pagenum, TRUE) != PFE_OK) { PF_PrintError("PF_UnfixPage"); exit(1); }
    }
    
    if (PF_CloseFile(fd) != PFE_OK) { PF_PrintError("PF_CloseFile"); exit(1); }


    /* --- 3. Run Workload --- */
    if ((fd = PF_OpenFile(TESTFILE)) < 0) { PF_PrintError("PF_OpenFile"); exit(1); }
    
    PF_ResetStats();
    
    for (i = 0; i < TOTAL_ACCESSES; i++) {
        /* Get a random page */
        int pageNum = rand() % FILE_SIZE; 
        
        /* Get a random operation type (read or write) */
        double op_type = (double)rand() / (double)RAND_MAX;

        if (PF_GetThisPage(fd, pageNum, &buf) != PFE_OK) {
            PF_PrintError("PF_GetThisPage");
            exit(1);
        }

        if (op_type < write_mix) {
            /* This is a "write" operation */
            if (PF_MarkDirty(fd, pageNum) != PFE_OK) {
                PF_PrintError("PF_MarkDirty");
                exit(1);
            }
            PF_UnfixPage(fd, pageNum, TRUE); /* Mark as dirty */
        } else {
            /* This is a "read" operation */
            PF_UnfixPage(fd, pageNum, FALSE); /* Not dirty */
        }
    }
    
    /* --- 4. Report Results --- */
    print_stats(strategy_str, write_mix);

    /* --- 5. Cleanup --- */
    PF_CloseFile(fd);
    PF_DestroyFile(TESTFILE);
    
    return 0;
}