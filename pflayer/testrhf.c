/* testrhf.c: Test program for the RHF layer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "rhf.h"

#define SLOTTED_FILE "students_slotted.db"
#define NUM_RECORDS 1000
#define MIN_NAME_LEN 10
#define MAX_NAME_LEN 50

/* A variable-length student record */
typedef struct {
    int studentID;
    float gpa;
    /* Name is variable length */
    char name[MAX_NAME_LEN + 1]; 
} Student;

/*
 * Generates a random string (name)
 */
void get_random_name(char *name)
{
    int len = MIN_NAME_LEN + (rand() % (MAX_NAME_LEN - MIN_NAME_LEN + 1));
    int i;
    for (i = 0; i < len; i++) {
        name[i] = 'a' + (rand() % 26);
    }
    name[len] = '\0'; /* Null terminate */
}

/*
 * Returns the actual storage size of a student record
 */
int get_record_size(Student *s)
{
    /* size = sizeof(int) + sizeof(float) + (name_length + 1 for null) */
    return sizeof(int) + sizeof(float) + strlen(s->name) + 1;
}

void run_tests()
{
    int fd, error;
    int i;
    Student s;
    RID rid;
    int totalSlottedPages = 0;

    printf("--- 1. Testing RHF Layer ---\n");
    srand(time(NULL));
    RHF_DestroyFile(SLOTTED_FILE); /* Clean up previous runs */

    if ((error = RHF_CreateFile(SLOTTED_FILE)) != RHF_OK) {
        RHF_PrintError("RHF_CreateFile", error); exit(1);
    }
    if ((fd = RHF_OpenFile(SLOTTED_FILE)) < 0) {
        RHF_PrintError("RHF_OpenFile", PFerrno); exit(1);
    }

    printf("Inserting %d variable-length student records...\n", NUM_RECORDS);
    
    RID* rids = (RID*)malloc(sizeof(RID) * NUM_RECORDS);
    int* record_sizes = (int*)malloc(sizeof(int) * NUM_RECORDS);
    long totalDataSize = 0;

    for (i = 0; i < NUM_RECORDS; i++)
    {
        s.studentID = i;
        s.gpa = (float)(rand() % 400) / 100.0;
        get_random_name(s.name);
        
        int len = get_record_size(&s);
        record_sizes[i] = len;
        totalDataSize += len;
        
        if ((error = RHF_InsertRecord(fd, (char*)&s, len, &rids[i])) != RHF_OK) {
            RHF_PrintError("RHF_InsertRecord", error);
            RHF_CloseFile(fd);
            exit(1);
        }
        
        /* Track the max page number */
        if (rids[i].pageNum > totalSlottedPages) {
            totalSlottedPages = rids[i].pageNum;
        }
    }
    totalSlottedPages++; /* 0-indexed, so add 1 */

    printf("Insertion complete. Total pages used: %d\n", totalSlottedPages);
    printf("Total raw data size: %ld bytes\n", totalDataSize);
    
    /* Test Scan */
    printf("\nTesting RHF_Scan...\n");
    RHF_Scan scan;
    int scan_count = 0;
    char recBuf[sizeof(Student)];
    int recLen;
    RID recRID;
    
    RHF_StartScan(fd, &scan);
    while (RHF_GetNextRecord(&scan, recBuf, &recLen, &recRID) == RHF_OK)
    {
        scan_count++;
    }
    RHF_EndScan(&scan);
    printf("Scan complete. Found %d records (expected %d).\n", scan_count, NUM_RECORDS);

    /* Test Delete */
    printf("\nTesting RHF_DeleteRecord (deleting even-numbered IDs)...\n");
    int delete_count = 0;
    for (i = 0; i < NUM_RECORDS; i += 2)
    {
        if (RHF_DeleteRecord(fd, &rids[i]) == RHF_OK) {
            delete_count++;
        }
    }
    printf("Deleted %d records.\n", delete_count);

    /* Test Scan again */
    printf("Running scan again...\n");
    scan_count = 0;
    RHF_StartScan(fd, &scan);
    while (RHF_GetNextRecord(&scan, recBuf, &recLen, &recRID) == RHF_OK)
    {
        scan_count++;
    }
    RHF_EndScan(&scan);
    printf("Scan complete. Found %d records (expected %d).\n", scan_count, NUM_RECORDS - delete_count);
    
    if (RHF_CloseFile(fd) != RHF_OK) {
        RHF_PrintError("RHF_CloseFile", PFerrno); exit(1);
    }
    
    free(rids);
    
    /* --- 2. Performance Comparison --- */
    printf("\n--- 2. Space Utilization Comparison ---\n");
    
    /* Static record lengths to test */
    int static_lengths[] = {64, 128, 256};
    int num_static_lengths = 3;
    long slotted_file_size = (long)totalSlottedPages * PF_PAGE_SIZE;
    
    printf("\n");
    printf("+--------------------------+---------------+--------------+-----------------+\n");
    printf("| Management Method        | Record Size   | Total Pages  | Total File Size |\n");
    printf("+--------------------------+---------------+--------------+-----------------+\n");
    printf("| Slotted Page (Variable)  | Avg: %-7.1f | %-12d | %-15ld |\n", 
           (double)totalDataSize / NUM_RECORDS, 
           totalSlottedPages, 
           slotted_file_size);

    for (i = 0; i < num_static_lengths; i++)
    {
        int max_len = static_lengths[i];
        
        /* Check if this max_len is valid for our data */
        int overhead = sizeof(int) + sizeof(float) + 1; /* ID, GPA, null char */
        if (max_len < (MAX_NAME_LEN + overhead)) {
             printf("| Static (Fixed Padding)   | %-13d | (Insufficient) | (N/A)           |\n", max_len);
             continue;
        }

        /* Calculate how many static records fit on one page */
        int recs_per_page = (int)floor((double)PF_PAGE_SIZE / max_len);
        if (recs_per_page == 0) {
            printf("| Static (Fixed Padding)   | %-13d | (Too Large)    | (N/A)           |\n", max_len);
            continue;
        }
        
        int total_static_pages = (int)ceil((double)NUM_RECORDS / recs_per_page);
        long total_static_size = (long)total_static_pages * PF_PAGE_SIZE;

        printf("| Static (Fixed Padding)   | %-13d | %-12d | %-15ld |\n", 
               max_len, 
               total_static_pages, 
               total_static_size);
    }
    printf("+--------------------------+---------------+--------------+-----------------+\n");

    /* Calculate and print space efficiency */
    long slotted_overhead = slotted_file_size - totalDataSize;
    printf("\nSpace Efficiency:\n");
    printf("  Slotted Page: Total Size = %ld, Data = %ld, Overhead = %ld (%.1f%%)\n",
           slotted_file_size, totalDataSize, slotted_overhead,
           (double)slotted_overhead / slotted_file_size * 100.0);

    free(record_sizes);
}

int main()
{
    PF_Init();
    run_tests();
    return 0;
}