/* pf.h: externs and error codes for Paged File Interface*/
#ifndef TRUE
#define TRUE 1		
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Page Replacement Strategies */
#define PF_STRAT_LRU 0
#define PF_STRAT_MRU 1

/************** Error Codes *********************************/
#define PFE_OK		0	/* OK */
#define PFE_NOMEM	-1	/* no memory */
#define PFE_NOBUF	-2	/* no buffer space */
#define PFE_PAGEFIXED 	-3	/* page already fixed in buffer */
#define PFE_PAGENOTINBUF -4	/* page to be unfixed is not in the buffer */
#define PFE_UNIX	-5	/* unix error */
#define PFE_INCOMPLETEREAD -6	/* incomplete read of page from file */
#define PFE_INCOMPLETEWRITE -7	/* incomplete write of page to file */
#define PFE_HDRREAD	-8	/* incomplete read of header from file */
#define PFE_HDRWRITE	-9	/* incomplte write of header to file */
#define PFE_INVALIDPAGE -10	/* invalid page number */
#define PFE_FILEOPEN	-11	/* file already open */
#define	PFE_FTABFULL	-12	/* file table is full */
#define PFE_FD		-13	/* invalid file descriptor */
#define PFE_EOF		-14	/* end of file */
#define PFE_PAGEFREE	-15	/* page already free */
#define PFE_PAGEUNFIXED	-16	/* page already unfixed */

/* Internal error: please report to the TA */
#define PFE_PAGEINBUF	-17	/* new page to be allocated already in buffer */
#define PFE_HASHNOTFOUND -18	/* hash table entry not found */
#define PFE_HASHPAGEEXIST -19	/* page already exist in hash table */


/* page size */
#define PF_PAGE_SIZE	4096

/* externs from the PF layer */
extern int PFerrno;		/* error number of last error */
extern void PF_Init();
extern int PF_CreateFile(char *fname);
extern int PF_DestroyFile(char *fname);
extern int PF_OpenFile(char *fname);
extern int PF_CloseFile(int fd);
extern int PF_GetFirstPage(int fd, int *pagenum, char **pagebuf);
extern int PF_GetNextPage(int fd, int *pagenum, char **pagebuf);
extern int PF_GetThisPage(int fd, int pagenum, char **pagebuf);
extern int PF_AllocPage(int fd, int *pagenum, char **pagebuf);
extern int PF_DisposePage(int fd, int pagenum);
extern int PF_UnfixPage(int fd, int pagenum, int dirty);
extern void PF_PrintError(char *s);

/* --- NEW FUNCTIONS TO BE ADDED --- */

/**
 * @brief Sets the size of the buffer pool.
 * Must be called before any files are opened.
 * @param size Number of pages in the buffer pool.
 */
extern void PF_SetBufferSize(int size);

/**
 * @brief Sets the global page replacement strategy.
 * @param strategy PF_STRAT_LRU or PF_STRAT_MRU.
 */
extern void PF_SetStrategy(int strategy);

/**
 * @brief Explicitly marks a fixed page as dirty.
 * This also moves it to the most-recently-used position in the buffer.
 * @param fd File descriptor.
 * @param pagenum Page number to mark.
 * @return PFE_OK on success, or an error code.
 */
extern int PF_MarkDirty(int fd, int pagenum);

/**
 * @brief Resets the I/O statistics counters to zero.
 */
extern void PF_ResetStats(void);

/**
 * @brief Retrieves the current I/O statistics.
 * @param logical_reads Count of all page requests (hits + misses).
 * @param physical_reads Count of pages read from disk.
 * @param physical_writes Count of pages written to disk.
 * @return PFE_OK.
 */
extern int PF_GetStats(long *logical_reads, 
                       long *physical_reads, 
                       long *physical_writes);