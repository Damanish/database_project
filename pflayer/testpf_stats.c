/* testpf_stats.c: Tests strategies and statistics */
#include <stdio.h>
#include "pf.h"
#include <stdlib.h>

#define TESTFILE "testfile_stats"
#define BUFFER_SIZE 5
#define FILE_SIZE 7

/*
 * This workload demonstrates the difference between LRU and MRU
 * for a cyclical, sequential scan.
 *
 * We use a buffer size of 5 and a file size of 7.
 * We will access pages 0, 1, 2, 3, 4, 5, 6 (fill buffer + 2 evictions)
 * Then, we access 0, 1, 2, 3, 4, 5, 6 again (scan)
 *
 * With LRU:
 * - Access 0-4: 5 misses (fill buffer)
 * - Access 5: miss (evict 0)
 * - Access 6: miss (evict 1)
 * - Buffer is now [6, 5, 4, 3, 2] (MRU...LRU)
 * - Access 0: miss (evict 2)
 * - Access 1: miss (evict 3)
 * - Every access in the scan is a miss. Total = 14 misses.
 *
 * With MRU:
 * - Access 0-4: 5 misses (fill buffer)
 * - Access 5: miss (evict 4)
 * - Access 6: miss (evict 5)
 * - Buffer is now [6, 3, 2, 1, 0] (MRU...LRU)
 * - Access 0: HIT
 * - Access 1: HIT
 * - Access 2: HIT
 * - Access 3: HIT
 * - Access 4: miss (evict 6)
 * - Total = 7 initial misses + 2 scan misses = 9 misses.
 */
void run_workload(int fd)
{
	int i;
	int pagenum;
	char *buf;
	int error;

	printf("--- Running Workload ---\n");
	
	/* First scan (populating) */
	printf("Populating scan (pages 0-%d):\n", FILE_SIZE - 1);
	for (i = 0; i < FILE_SIZE; i++)
	{
		if ((error=PF_GetThisPage(fd, i, &buf)) != PFE_OK)
		{
			PF_PrintError("PF_GetThisPage");
			exit(1);
		}
		printf("  Got page %d. (Value: %d)\n", i, *buf);
		
		/* Mark page as 'dirty' to test explicit call */
		if ((error=PF_MarkDirty(fd, i)) != PFE_OK)
		{
			PF_PrintError("PF_MarkDirty");
			exit(1);
		}
		
		if ((error=PF_UnfixPage(fd, i, TRUE)) != PFE_OK)
		{
			PF_PrintError("PF_UnfixPage");
			exit(1);
		}
	}
	
	/* Second scan (testing cache) */
	printf("Testing scan (pages 0-%d):\n", FILE_SIZE - 1);
	for (i = 0; i < FILE_SIZE; i++)
	{
		if ((error=PF_GetThisPage(fd, i, &buf)) != PFE_OK)
		{
			PF_PrintError("PF_GetThisPage");
			exit(1);
		}
		printf("  Got page %d. (Value: %d)\n", i, *buf);
		
		if ((error=PF_UnfixPage(fd, i, FALSE)) != PFE_OK)
		{
			PF_PrintError("PF_UnfixPage");
			exit(1);
		}
	}
}


void print_stats()
{
	long logical, physical_r, physical_w;
	PF_GetStats(&logical, &physical_r, &physical_w);
	
	printf("\n--- STATISTICS ---\n");
	printf("Logical I/O:     %ld\n", logical);
	printf("Physical Reads:  %ld\n", physical_r);
	printf("Physical Writes: %ld\n", physical_w);
	
	long hits = logical - (physical_r); /* Note: Alloc is a 'miss' */
	if (logical > 0)
	{
		printf("Hit Rate (reads):  %.2f%%\n", 100.0 * hits / logical);
	}
	printf("--------------------\n\n");
}


int main()
{
	int fd, i, error;
	int pagenum;
	char *buf;

	/* Set buffer size *before* PF_Init */
	PF_SetBufferSize(BUFFER_SIZE);
	printf("Set buffer size to %d\n", BUFFER_SIZE);

	PF_Init();

	/* Create and populate the file */
	if ((error=PF_CreateFile(TESTFILE)) != PFE_OK)
	{
		PF_PrintError("PF_CreateFile");
		exit(1);
	}
	if ((fd=PF_OpenFile(TESTFILE)) < 0)
	{
		PF_PrintError("PF_OpenFile");
		exit(1);
	}
	for (i = 0; i < FILE_SIZE; i++)
	{
		if ((error=PF_AllocPage(fd, &pagenum, &buf)) != PFE_OK)
		{
			PF_PrintError("PF_AllocPage");
			exit(1);
		}
		*buf = i; /* Write page number into page data */
		if ((error=PF_UnfixPage(fd, pagenum, TRUE)) != PFE_OK)
		{
			PF_PrintError("PF_UnfixPage");
			exit(1);
		}
	}
	if ((error=PF_CloseFile(fd)) != PFE_OK)
	{
		PF_PrintError("PF_CloseFile");
		exit(1);
	}
	printf("Created file '%s' with %d pages.\n\n", TESTFILE, FILE_SIZE);


	/*
	 * === TEST 1: LRU STRATEGY ===
	 */
	printf("************************\n");
	printf("* TESTING LRU       *\n");
	printf("************************\n");
	PF_SetStrategy(PF_STRAT_LRU);
	
	if ((fd=PF_OpenFile(TESTFILE)) < 0)
	{
		PF_PrintError("PF_OpenFile LRU");
		exit(1);
	}
	
	PF_ResetStats();
	/* Note: Alloc pages are 7 logical reads + 7 physical reads */
	printf("Stats reset. Running LRU workload...\n");
	
	run_workload(fd);
	
	print_stats();
	
	if ((error=PF_CloseFile(fd)) != PFE_OK)
	{
		PF_PrintError("PF_CloseFile LRU");
		exit(1);
	}
	
	
	/*
	 * === TEST 2: MRU STRATEGY ===
	 */
	printf("************************\n");
	printf("* TESTING MRU       *\n");
	printf("************************\n");
	PF_SetStrategy(PF_STRAT_MRU);
	
	if ((fd=PF_OpenFile(TESTFILE)) < 0)
	{
		PF_PrintError("PF_OpenFile MRU");
		exit(1);
	}
	
	PF_ResetStats();
	printf("Stats reset. Running MRU workload...\n");
	
	run_workload(fd);
	
	print_stats();
	
	if ((error=PF_CloseFile(fd)) != PFE_OK)
	{
		PF_PrintError("PF_CloseFile MRU");
		exit(1);
	}
	
	/* Clean up */
	PF_DestroyFile(TESTFILE);
	printf("Cleaned up %s.\n", TESTFILE);
}